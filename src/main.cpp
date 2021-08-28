#include <stdlib.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <map>

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
const auto bundle_index_filename = "bundle-index.json";

typedef struct wydecontext {
    fs::path path_wyderoot;
    fs::path path_god;
    std::map<std::string, fs::path> bundles;
} wydecontext;

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
		std::cout << "ERROR: path doesn't exists: " << wyderoot_path << '\n';
		return -2;
	}

	// check if god path exists
	fs::path god_path = wyderoot_path.append("god");
	if (!fs::exists(god_path)) {
		std::cout << "ERROR: path doesn't exists: " << god_path << '\n';
		return -3;
	}

	wp.path_wyderoot = std::move(wyderoot_path);
	wp.path_god = std::move(god_path);

	return 0;
}

int main ()
{
	wydecontext wydectx;

	if (auto ret = init(wydectx) > 0) {
		return ret;
	}

	for (auto &bundle_path: fs::recursive_directory_iterator{wydectx.path_god}) {
		if (bundle_path.is_directory()) {
			auto bundle = fs::relative(bundle_path, wydectx.path_god);
			wydectx.bundles[bundle.generic_string()] = std::move(bundle_path);
		}
	}

	for (auto &p: wydectx.bundles)
		std::cout << p.first << ": path=" << p.second << '\n';



	return 0;
}
