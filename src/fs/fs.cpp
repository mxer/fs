#include "fs/fs.h"
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>
#include <stdlib.h>
#include <cstdlib>
#include <stdio.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <fstream>

namespace fs {
	char path::PathDelim = '/';

	std::string simplify(const std::string &path) {
		std::vector<std::string> pathItems;
		std::vector<std::string> result;
		std::stringstream ss;

		{
			std::string item;
			ss.str(path);
			while (std::getline(ss, item, path::PathDelim)) {
				pathItems.push_back(item);
			}
		}

		std::reverse(pathItems.begin(), pathItems.end());
		for (auto it = pathItems.begin(); it != pathItems.end(); ++it) {
			auto item = *it;

			if ((item == "") || (item == ".")) {
				continue;
			}

			if (item == "..") {
				++it;
				continue;
			}

			result.push_back(item);
		}
		std::reverse(result.begin(), result.end());

		std::stringstream resultPath;

		if (path[0] == '/') {
			resultPath << '/';
		}

		auto it = result.begin();
		resultPath << *it;
		++it;
		for (; it != result.end(); ++it) {
			resultPath << '/' << *it;
		}

		return resultPath.str();
	}


	path::path() {

	}

	path::path(const std::string &path) {
		if (path.size() == 0) {
			throw "Path cannot be empty.";
		}

		auto newPath = path;

		char end = newPath[newPath.size() - 1];
		if ((end == '/') || (end == '\\')) {
			newPath = newPath.substr(0, newPath.size() - 1);
		}

		auto delim = newPath.find_last_of("/\\");
		if (delim == std::string::npos) {
			itemPath = "";
			itemName = newPath;
		} else {
			itemPath = newPath.substr(0, delim);
			itemName = newPath.substr(delim + 1);
		}
	}

	path::~path() {

	}

	bool path::create() {
		auto handle = fopen(full_name().c_str(), "wb");

		if (handle != NULL) {
			fclose(handle);

			return true;
		}

		return false;
	}

	bool path::mkdir() {
		return ::mkdir(full_name().c_str(), 0755) == 0;
	}

	bool path::mkdirs() {
		if (exists()) {
			return true;
		}

		return parent().mkdirs() && mkdir();
	}

	bool path::remove() {
		::remove(full_name().c_str());
	}

	bool path::exists() const {
		struct stat st;
		return stat(full_name().c_str(), &st) == 0;
	}

	bool path::is_file() const {
		struct stat st;

		if (stat(full_name().c_str(), &st) != 0) {
			return false;
		}

		return S_ISREG(st.st_mode);
	}

	bool path::is_directory() const {
		struct stat st;

		if (stat(full_name().c_str(), &st) != 0) {
			return false;
		}

		return S_ISDIR(st.st_mode);
	}

	bool path::is_temponary() const {
		std::string templ;

		{
			auto temp = getenv("TMPDIR");

			if (temp == NULL) {
				templ = "/tmp";
			} else {
				templ = temp;
			}
		}

		return (full_name().size() > templ.size()) && (full_name().substr(0, templ.size()) == templ);
	}

	path path::parent() const {
		return path{dir()};
	}

	path &path::parent(const path &target) {
		if (is_directory()) {
			if (target.is_directory()) {
				rename(full_name().c_str(), (target.full_name() + PathDelim + name()).c_str());
			} else if (target.is_file()) {
				rename(full_name().c_str(), target.full_name().c_str());
			}
		} else if (is_file()) {
			if (target.is_directory()) {
				rename(full_name().c_str(), (target.full_name() + PathDelim + name().c_str()).c_str());
			} else if (target.is_file()) {
				rename(full_name().c_str(), target.full_name().c_str());
			}
		}

		return *this;
	}

	// Stat functions
	unsigned long path::createdAt() {
		struct stat st;

		if (stat(full_name().c_str(), &st) != 0) {
			return 0;
		}

#ifdef HAVE_ST_BIRTHTIME
		return (unsigned long)st.st_birthtime;
#else
		return (unsigned long)st.st_ctime;
#endif
	}

	unsigned long path::modifiedAt() {
		struct stat st;

		if (stat(full_name().c_str(), &st) != 0) {
			return 0;
		}

		return (unsigned long)st.st_mtime;
	}

	// Folder functions
	path path::add(const std::string &name) {
		path{name}.parent(*this);
	}

	std::vector<path> path::list() const {
		auto handle = opendir(full_name().c_str());

		if (handle == NULL) {
			return {};
		}

		std::vector<path> files;

		while (auto child = readdir(handle)) {
			if ((strcmp(child->d_name, ".") == 0) || (strcmp(child->d_name, "..")) == 0) {
				continue;
			}

			files.push_back((*this)/child->d_name);
		}

		closedir(handle);

		return files;
	}

	std::string path::relative(const path &child) const {
		auto name = full_name(),
			childName = child.full_name();

		if ((name.length() >= childName.length()) || (childName.substr(0, name.length()) != name)) {
			return childName;
		}

		return childName.substr(name.length() + 1);
	}

	// File functions
	std::string path::content() const {
		auto handle = fopen(full_name().c_str(), "rb");

		if (handle == NULL) {
			return {};
		}

		fseek(handle, 0, SEEK_END);
		auto size = (unsigned long)ftell(handle);
		fseek(handle, 0, SEEK_SET);

		std::string buffer;
		buffer.resize(size);

		fread(&buffer[0], sizeof(buffer[0]), size, handle);
		fclose(handle);

		return buffer;
	}

	bool path::content(const std::string &newContent) {
		auto handle = fopen(full_name().c_str(), "wb");

		if (handle != NULL) {
			fwrite(newContent.data(), sizeof(newContent[0]), newContent.size(), handle);
			fclose(handle);

			return true;
		}

		return false;
	}

	path &path::open(std::ifstream &stream) {
		stream.open(full_name());
		return *this;
	}

	path &path::open(std::ofstream &stream) {
		stream.open(full_name());
		return *this;
	}

	path &path::open(std::fstream &stream) {
		stream.open(full_name());
		return *this;
	}

	const path &path::open(std::ifstream &stream) const {
		stream.open(full_name());
		return *this;
	}

	const path &path::open(std::ofstream &stream) const {
		stream.open(full_name());
		return *this;
	}

	const path &path::open(std::fstream &stream) const {
		stream.open(full_name());
		return *this;
	}

	// Operators
	path path::operator /(const std::string &sub) const {
		return path{itemPath + PathDelim + itemName + PathDelim + sub};
	}

	// To use as map key
	bool path::operator <(const path &operand) const {
		return full_name() < operand.full_name();
	}

	path temp() {
		std::string templ;

		{
			auto temp = getenv("TMPDIR");

			if (temp == NULL) {
				templ = "/tmp";
			} else {
				templ = temp;
			}
		}

		templ += "/yaplc.";


		path temp;
		auto t = (unsigned long)time(NULL);

		do {
			char buf[64];
			int c = snprintf(buf, 64, "%lu", t);

			temp = templ + buf;
		} while (temp.exists());

		temp.mkdirs();

		return temp;
	}

	path relative(const std::string &relative) {
		if (relative[0] == '/') {
			return path{simplify(relative)};
		}

		char working_dir[PATH_MAX];
		getcwd(working_dir, PATH_MAX);

		return path{simplify(std::string(working_dir) + "/" + relative)};
	}

	std::string escape(const path &path) {
		return escape(path.full_name());
	}

	std::string escape(const std::string &path) {
		std::string result;
		result.reserve(path.size());

		for (auto c : path) {
			switch (c) {
			case '\\':
				result += "\\\\";
				break;
			case '\'':
				result += "\\\'";
				break;
			case '\"':
				result += "\\\"";
				break;
			default:
				result += c;
			}
		}

		return result;
	}
}
