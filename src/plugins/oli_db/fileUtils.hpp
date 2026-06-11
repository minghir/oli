#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>
#include <filesystem>
#include <iostream>
#include <map>




//	namespace fs = std::filesystem;


	//extern std::string csv_directory_path;
	//extern std::string pdf_directory_path;
	//extern std::string reports_directory_path;

	std::wstring wstr_read_RTF_file(const std::string& filePath);

	void convertRTFtoPDF(const std::string& rtfFile, std::string& pdfFile);
	void wconvertRTFtoPDF(const std::string& rtfFile, const std::string& pdfFile);
	void initLibreOffice();
	bool copyFile(const std::string& source, const std::string& destination);
	bool removeFile(const std::string& source);
	bool renameFile(const std::string& oldName, const std::string& newName);
	bool wrenameFile(const std::wstring& oldName, const std::wstring& newName);
	bool wcopyFile(const std::wstring& source, const std::wstring& destination);

	void create_dir_if_missing(const std::string& path);
	std::string getCurrentDirectory();
	std::vector<std::wstring> readCSVFile(const std::string& filename);
	std::vector<std::wstring> readCSVFile2(const std::string& filename);
	std::vector<std::string> getCsvFiles(const std::string& folderPath);
	bool load_options_file(std::string cfg_filepath);

	std::string getFilenameWithoutExtension(const std::string& filepath);
	bool fileExists(const std::wstring& path);
	bool directoryExists(const std::wstring& path);
	std::vector<std::wstring> getFilesWithExtension(const std::wstring& directory, const std::wstring& extension);
	std::wstring get_file_name(const std::wstring& path);
	bool delete_files_by_extension(const std::wstring& dir_path, const std::wstring& extensie);


	bool populate_config_map_from_file(const std::string& cfg_filepath, std::map<std::wstring, std::wstring>& out_map);
	std::wstring getFilenameWithoutExtension(const std::wstring& filepath);

	std::wstring getTempPath();
	std::wstring getUniqueTempFilePath(const std::wstring& tempDir, const std::wstring& prefix);

	std::wstring ensureExtension(const std::wstring& name, const std::wstring& extension);


#endif

