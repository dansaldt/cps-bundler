#include <iostream>
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

static const char* OPT_CACHE = "--cache";
static const char* OPT_CACHE_SHORT = "-c";
static const char* OPT_CACHE_OFF = "off";
static const char* CACHED_FILE_NAME = "CpsBundlerCache.txt";

static const char* OPT_FILES = "--files";
static const char* OPT_FILES_SHORT = "-f";

static inline std::string_view get_opt_val(std::string_view s, std::string_view opt) {
    auto opt_len = strlen(opt.data());
    auto ret = s.substr(opt_len + 1, strlen(s.data()) - opt_len);
    return ret;
}

static inline bool is_opt_set(std::string_view s, std::string_view opt, std::string_view opt_short) {

    auto opt_cache = s.find(opt);
    if (opt_cache != std::string::npos) {
        auto val = get_opt_val(s, OPT_CACHE);
        if (val.compare(OPT_CACHE_OFF) == 0) {
            return true;
        }
    }

    opt_cache = s.find(opt_short);
    if (opt_cache != std::string::npos) {
        auto val = get_opt_val(s, opt_short);
        if (val.compare(OPT_CACHE_OFF) == 0) {
            return true;
        }
    }

    return false;
}

int override_options(context &ctx, int argc, char **argv) {

    for (int i = 2; i < argc; ++i) {
        std::string_view optval = argv[i];

        // option --cache | -c
        if (is_opt_set(optval,
                /*opt*/ OPT_CACHE,
                /*opt_short*/ OPT_CACHE_SHORT)) {
            ctx.cached = true;
            continue;
        }

        // option --files | -f
        if (is_opt_set(optval,
                /*opt*/ OPT_FILES,
                /*opt_short*/ OPT_FILES_SHORT)) {
            // TODO
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
    if (fs::exists(cache_path)) {
        ctx.path_cache = std::move(cache_path);
    }

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
        std::cout << "options: -e | --append-extension, default searching for .god files.\n";
        std::cout << "         -E | --extension, override default and append additional extension.\n";
        std::cout << "         -f | --files, move only selected files instead of all files in current directory,\n";
        std::cout << "                       multiple files are comma separated.\n";
        std::cout << "         -c | --cache, cache files which are moved to eWamSrcPath in ./OcsBundlerCache.txt,\n";
        std::cout << "                       by default cache is on, to turn off set --cache=off\n";
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
    for (auto[file, dest]: ctx.caches) {
        auto src = ctx.files2move[file];
        if (!fs::exists(src))
            continue;

        if (!fs::exists(dest) && !fs::create_directory(dest)) {
            std::cout << "ERROR> "
                      << file << " cached to dir " << dest
                      << ", but dir doesn't exists and cannot create one\n";
            continue;
        }

        auto dest_file = fs::path(dest);
        dest_file.append(file);

        std::error_code ec;
        fs::rename(src, dest_file, ec);

        if (ec.value() != 0) {
            std::cout << "ERROR> failed to move file "
                      << file << " to " << dest
                      << ": " << ec.message() << "\n";
            continue;
        }

        ctx.files_moved[file] = true;
    }

    // recursively search for files in the target path to move into
    for (auto &p: fs::recursive_directory_iterator{ctx.path_target}) {
        // TODO
    }

    // TODO write to cache if necessary

    return 0;
}
