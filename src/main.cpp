#include <iostream>
#include <cstdio>
#include <filesystem>
#include <map>
#include <fstream>
#include <regex>
#include <string>
#include <cctype>

namespace fs = std::filesystem;

typedef struct context {
    fs::path path_target;
    fs::path path_cache;
    fs::path extension;
    std::map<std::string, fs::path> files2move;
    std::map<std::string, bool> files_moved;
    std::map<std::string, fs::path> caches;
    bool cached = true;
} context;

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

static const char *OPT_CACHE = "--cache";
static const char *OPT_CACHE_SHORT = "-c";
static const char *OPT_CACHE_OFF = "off";
static const char *CACHED_FILE_NAME = "CpsBundlerCache.txt";

static const char *OPT_FILES = "--files";
static const char *OPT_FILES_SHORT = "-f";

static inline std::string_view get_opt_val(std::string_view s, std::string_view opt) {
    auto opt_len = strlen(opt.data());
    auto ret = s.substr(opt_len + 1, strlen(s.data()) - opt_len);
    return ret;
}

static inline std::string_view opt_val_if_set(
        std::string_view s,
        bool *is_set,
        std::string_view opt, std::string_view opt_short) {

    auto opt_cache = s.find(opt);
    if (opt_cache != std::string::npos) {
        *is_set = true;
        return get_opt_val(s, opt);
    }

    opt_cache = s.find(opt_short);
    if (opt_cache != std::string::npos) {
        *is_set = true;
        return get_opt_val(s, opt_short);
    }

    return "";
}

int override_options(context &ctx, int argc, char **argv) {

    for (int i = 2; i < argc; ++i) {
        std::string_view optval = argv[i];

        // option --cache | -c
        bool opt_cache_set = false;
        auto val = opt_val_if_set(optval, &opt_cache_set,
                /*opt*/ OPT_CACHE,
                /*opt_short*/ OPT_CACHE_SHORT);
        if (opt_cache_set) {
            if (val.compare(OPT_CACHE_OFF) == 0) {
                ctx.cached = false;
            }
            continue;
        }
    }

    return 0;
}

int init(context &ctx, int argc, char **argv) {

    // check target dir if exists
    fs::path target_path = {argv[1]};
    if (!fs::exists(target_path)) {
        std::cout << "ERROR> eWamSrcPath doesn't exists or invalid: " << argv[1] << '\n';
        return -1;
    }

    // get cache file in current directory
    fs::path cache_path = fs::current_path();
    cache_path.append(CACHED_FILE_NAME);

    // initialize context
    ctx.path_target = std::move(target_path);
    ctx.cached = true;
    ctx.extension = fs::path{".god"};
    ctx.path_cache = std::move(cache_path);

    // check options and override if necessary
    if (auto ret = override_options(ctx, argc, argv) != 0) {
        return ret;
    }

    return 0;
}


int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " eWamSrcPath [options]\n";
        std::cout << "eWamSrcPath: target directory where files are moved to.\n";
        std::cout << "options:\n";
        std::cout << "    -e | --append-extension, default searching for .god files.\n";
        std::cout << "    -E | --extension, override default and append additional extension.\n";
        std::cout << "    -f | --files, move only selected files instead of all files in current directory,\n";
        std::cout << "                  multiple files are comma separated.\n";
        std::cout << "    -c | --cache, cache files which are moved to eWamSrcPath in ./OcsBundlerCache.txt,\n";
        std::cout << "                  by default cache is on, to turn off set --cache=off\n";
        std::cout << std::endl;

        return -1;
    }

    context ctx;

    if (auto ret = init(ctx, argc, argv) != 0) {
        return ret;
    }

    // get files to move
    for (auto &file: fs::directory_iterator{fs::current_path()}) {
        if (file.is_regular_file() && ctx.extension.compare(file.path().extension()) == 0) {
            auto path = file.path();
            auto name = path.filename().string();
            ctx.files2move[std::move(name)] = std::move(path);
        }
    }
    if (ctx.files2move.empty()) {
        // nothing to move
        return 0;
    }

    // read cache if exists
    if (ctx.cached
        && fs::exists(ctx.path_cache)
        && fs::file_size(ctx.path_cache) > 0) {

        std::ifstream fcache(ctx.path_cache);
        // if cache file cannot be open, skip it
        if (fcache.is_open()) {
            char buf[8192];
            memset(buf, 0, sizeof(buf));
            while (fcache.getline(buf, sizeof(buf))) {
                std::string_view cacheline{buf};
                auto sep_pos = cacheline.find('=');
                if (sep_pos != std::string::npos) {
                    auto file = cacheline.substr(0, sep_pos);
                    auto cached_dest = cacheline.substr(sep_pos + 1);
                    auto cached_dest_path = fs::path(cached_dest);
                    if (fs::is_directory(cached_dest_path)) {
                        ctx.caches[std::string{file}] = std::move(cached_dest_path);
                    }
                }
                memset(buf, 0, sizeof(buf));
            }
            fcache.close();
        }
    }

    // move cached files directly to target directory
    for (auto[file, src]: ctx.files2move) {

        ctx.files_moved[file] = false;

        if (!fs::exists(src))
            continue;

        auto dest = ctx.caches[file];

        // if not cached string will be empty, skip
        if (dest.string().empty())
            continue;

        // check dest directory, create if non-existing
        if (!fs::exists(dest) && !fs::create_directory(dest)) {
            std::cout << "ERROR> "
                      << file << " cached to dir " << dest
                      << ", but dir doesn't exists and cannot create one\n";
            continue;
        }

        // make destination name
        auto dest_file = fs::path(dest);
        dest_file.append(file);

        // move
        std::error_code ec;
        fs::rename(src, dest_file, ec);

        // if there's an error, print err
        if (ec.value() != 0) {
            std::cout << "ERROR> failed to move file "
                      << file << " to " << dest
                      << ": " << ec.message() << "\n";
            continue;
        }

        // success, marked as moved and print info
        ctx.files_moved[file] = true;
        std::cout << "INFO> " << file << " => " << dest << '\n';
    }
    std::cout << std::flush;

    // get files still need to be moved because were not cached
    std::vector<fs::path> non_cached_fnames2_move;
    std::map<std::string, fs::path> fnames2cache;
    for (auto[file, moved]: ctx.files_moved) {
        if (!moved) {
            non_cached_fnames2_move.emplace_back(fs::path{file});
        }
    }
    // recursively search for files in the target path to move into
    if (!non_cached_fnames2_move.empty()) {
        for (auto &it: fs::recursive_directory_iterator{ctx.path_target}) {
            // directory or file with different extension not needed
            if (!it.is_regular_file())
                continue;
            if (ctx.extension.compare(it.path().extension()) != 0)
                continue;

            // compare
            const auto &p = it.path();
            auto f = p.filename();
            for (auto &fname: non_cached_fnames2_move) {
                if (fname.compare(f) == 0) {
                    auto name_str = fname.string();
                    auto &src = ctx.files2move[name_str];
                    std::error_code ec;

                    // move
                    fs::rename(src, p, ec);

                    // if there's an error, print err
                    if (ec.value() != 0) {
                        std::cout << "ERROR> failed to move file "
                                  << name_str << " to " << p
                                  << ": " << ec.message() << "\n";
                        continue;
                    }

                    // success, marked as moved and print info
                    ctx.files_moved[name_str] = true;
                    std::cout << "INFO> " << name_str << " => " << p << '\n';
                    fnames2cache[name_str] = p;
                }
            }

            if (non_cached_fnames2_move.empty())
                break;
        }
    }

    // write to cache if necessary
    if (ctx.cached && !non_cached_fnames2_move.empty()) {

        std::ofstream fcache{ctx.path_cache,
                             std::ios::out | std::ios::app};

        if (!fcache.is_open()) {
            std::cout << "ERROR> failed to write to cache: failed to open file: " << ctx.path_cache;
        } else {
            for (auto &fname: non_cached_fnames2_move) {
                auto name_str = fname.string();
                auto p = fnames2cache[name_str];
                p = p.remove_filename();
                fcache << name_str << "=" << p.string() << '\n';
            }
            fcache.close();
        }

    }

    // count files moved and failed
    size_t count_moved = 0;
    size_t count_move_failed = 0;
    for (auto[file, moved]: ctx.files_moved) {
        if (moved) {
            ++count_moved;
        } else {
            ++count_move_failed;
        }
    }

    // print summary
    std::cout << "finished, ";
    if (count_moved > 0) {
        std::cout << "moved " << count_moved << " file(s)";
    }
    if (count_move_failed > 0) {
        if (count_moved > 0) {
            std::cout << ", ";
        }
        std::cout << "failed to move " << count_move_failed << " file(s)";
    }
    std::cout << "." << std::endl;

    // pause console
    std::cout << "Press <ENTER> to continue . . .\n" << std::flush;
    std::getchar();

    return 0;
}
