#include <stdlib.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <map>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

void demo_exists (
	  const fs::path &p,
	  fs::file_status s = fs::file_status{} )
{
	std::cout << p;
	if (fs::status_known(s) ? fs::exists(s) : fs::exists(p)) {
		std::cout << " exits:\n";
	} else {
		std::cout << " does not exists\n";
	}
}

void test_filepath ()
{
	const fs::path target_path = {R"(C:\eWAM_6_1_5_32\eWAM\god)"};

	if (!fs::exists(target_path)) {
		std::cout << "target_path: \"" << target_path << "\" does not exist\n";
//				return -1;
	}

	std::cout << "target_path: " << target_path << '\n';
	std::cout << "root_name: " << target_path.root_name() << '\n';
	std::cout << "root_directory: " << target_path.root_directory() << '\n';
	std::cout << "root_path: " << target_path.root_path() << '\n';
	std::cout << "relative_path: " << target_path.relative_path() << '\n';
	std::cout << "parent_path: " << target_path.parent_path() << '\n';
	std::cout << "filename: " << target_path.filename() << '\n';
	std::cout << "stem: " << target_path.stem() << '\n';
	std::cout << "extension: " << target_path.extension() << '\n';

	for (auto &curr: fs::recursive_directory_iterator(target_path)) {
		if (curr.is_directory()) {
			std::cout << "dir: " << curr.path() << '\n';
		} else if (curr.is_regular_file()) {
//						std::cout << "file: ";
		} else {
//						std::cout << "other: ";
		}
//				std::cout << curr.path() << '\n';
	}

//		return 0;
}

const auto env_var_wyde_root = "WYDE-ROOT";
const auto bundle_index_filename = "bundle-index.txt";

typedef struct wydecontext {
    fs::path path_wyderoot;
    fs::path path_god;
    fs::path path_bundle_index;
    std::map<std::string, fs::path> bundles_path;
} wydecontext;

// trim from start (in place)
static inline void ltrim ( std::string &s )
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [] ( unsigned char ch ) {
	    return !std::isspace(ch);
	}));
}

// trim from end (in place)
static inline void rtrim ( std::string &s )
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [] ( unsigned char ch ) {
	    return !std::isspace(ch);
	}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim ( std::string &s )
{
	ltrim(s);
	rtrim(s);
}


int init ( wydecontext &wp )
{
	// get wyde-root environment variable
	const char *env_wyde_root = std::getenv(env_var_wyde_root);
	if (!env_wyde_root) {
		std::cout << "ERROR: environment variables \"" << env_var_wyde_root << "\" is not set\n";
		return -1;
	}

	// check if wyde-root path exists
	fs::path wyderoot_path = {env_wyde_root};
	if (!fs::exists(wyderoot_path)) {
		std::cout << "ERROR: path doesn't exists or invalid: " << wyderoot_path << '\n';
		return -2;
	}

	// check if god path exists
	fs::path god_path(wyderoot_path);
	god_path.append("god");
	if (!fs::exists(god_path)) {
		std::cout << "ERROR: path doesn't exists or invalid: " << god_path << '\n';
		return -3;
	}

	// check for bundle index json file in `wydectx.path_god`
	fs::path bundle_index_path(god_path);
	bundle_index_path.append(bundle_index_filename);
	if (!fs::exists(bundle_index_path)) {
		std::cout << "ERROR: path doesn't exists or invalid: " << bundle_index_path << '\n';
		return -4;
	} else if (!fs::is_regular_file(bundle_index_path)) {
		std::cout << "ERROR: is not valid file, expecting regurlar file: " << bundle_index_path << '\n';
		return -5;
	}

	wp.path_wyderoot = std::move(wyderoot_path);
	wp.path_god = std::move(god_path);
	wp.path_bundle_index = std::move(bundle_index_path);

	return 0;
}

int main ()
{
	wydecontext wydectx;

	if (auto ret = init(wydectx) != 0) {
		return ret;
	}

	for (auto &bundle_path: fs::recursive_directory_iterator{wydectx.path_god}) {
		if (bundle_path.is_directory()) {
			auto bundle = fs::relative(bundle_path, wydectx.path_god);
			wydectx.bundles_path[bundle.generic_string()] = std::move(bundle_path);
		}
	}

	// opening the bundle index file
	std::ifstream ifs_bundle_idx(wydectx.path_bundle_index);
	if (!ifs_bundle_idx.is_open()) {
		std::cout << "ERROR: failed to open file: " << wydectx.path_bundle_index << '\n';
		return -6;
	}
	// regex to identify which is bundle1, bundle2, class_or_module
	// by identifying prefix tabs
	std::regex bundle1("^\t[^\t]+");
	std::regex bundle2("^\t\t[^\t]+");
	std::regex class_or_modules("^\t\t\t[^\t]+");
	// loop each line in the file, only those line that match the regex are treated
	char buf[256];
	std::string cur_bundle1;
	std::string cur_bundle2;
	std::string cur_class_module;
	while (ifs_bundle_idx.getline(buf, sizeof(buf))) {
		if (std::regex_match(buf, class_or_modules)) {
			cur_class_module = buf;
			trim(cur_class_module);
			if (cur_bundle1.empty() || cur_bundle2.empty()) {
				std::cout << "ERROR: cannot put class/module \"" << cur_class_module << " to any bundle: "
					  << "cur_bundle1=\"" << cur_bundle1 << "\", "
					  << "cur_bundle2=\"" << cur_bundle2 << "\".\n";
			} else {
				std::cout << cur_bundle1 << "/" << cur_bundle2 << "\t" << cur_class_module << "\n";
			}
		} else if (std::regex_match(buf, bundle2)) {
			cur_bundle2 = buf;
			trim(cur_bundle2);
		} else if (std::regex_match(buf, bundle1)) {
			cur_bundle1 = buf;
			trim(cur_bundle1);
		}
	}
	// done, close bundle index file
	ifs_bundle_idx.close();

	return 0;
}
