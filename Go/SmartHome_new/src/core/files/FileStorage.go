package files

import (
	"archive/zip"
	"io"
	"os"
	"strconv"
	"strings"
)

const datesNew = "dates_new"

type FileProvider interface {
	GetName() string
	Read() ([]byte, error)
}

type osFileProvider struct {
	fileName string
	fullName string
}

func (p *osFileProvider) GetName() string {
	return p.fileName
}

func (p *osFileProvider) Read() ([]byte, error) {
	return os.ReadFile(p.fullName)
}

type zipFileProvider struct {
	fileName string
	file     *zip.File
}

func (p *zipFileProvider) GetName() string {
	return p.fileName
}

func (p *zipFileProvider) Read() ([]byte, error) {
	f, err := p.file.Open()
	if err != nil {
		return nil, err
	}
	defer f.Close()
	return io.ReadAll(f)
}

type FileStorage struct {
	Files     map[int][]FileProvider
	zipReader *zip.ReadCloser
}

func (s *FileStorage) Close() {
	if s.zipReader != nil {
		_ = s.zipReader.Close()
	}
}

func NewFileStorage(path, zipFileName string) (*FileStorage, error) {
	fileMap := make(map[int]map[string]FileProvider)
	err := buildOSFileMap(fileMap, path)
	if err != nil {
		return nil, err
	}
	reader, err := buildZipFileMap(fileMap, zipFileName)
	if err != nil {
		return nil, err
	}
	return &FileStorage{
		Files:     buildFileMap(fileMap),
		zipReader: reader,
	}, nil
}

func buildFileMap(fileMap map[int]map[string]FileProvider) map[int][]FileProvider {
	result := make(map[int][]FileProvider)
	for k, v := range fileMap {
		result[k] = buildFileArray(v)
	}
	return result
}

func buildFileArray(m map[string]FileProvider) []FileProvider {
	var result []FileProvider
	for _, v := range m {
		result = append(result, v)
	}
	return result
}

func buildOSFileMap(fileMap map[int]map[string]FileProvider, path string) error {
	path += string(os.PathSeparator) + datesNew
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()
	files, err := f.Readdir(-1)
	if err != nil {
		return err
	}
	for _, fi := range files {
		if fi.IsDir() {
			date, err := strconv.Atoi(fi.Name())
			if err != nil {
				return err
			}
			fileProviders, err := buildOSFileProviders(path + string(os.PathSeparator) + fi.Name())
			if err != nil {
				return err
			}
			if len(fileProviders) > 0 {
				fileMap[date] = fileProviders
			}
		}
	}
	return nil
}

func buildOSFileProviders(path string) (map[string]FileProvider, error) {
	result := make(map[string]FileProvider)
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	files, err := f.Readdir(-1)
	if err != nil {
		return nil, err
	}
	for _, fi := range files {
		result[fi.Name()] = &osFileProvider{fullName: path + string(os.PathSeparator) + fi.Name(), fileName: fi.Name()}
	}
	return result, nil
}

func buildZipFileMap(fileMap map[int]map[string]FileProvider, zipFileName string) (*zip.ReadCloser, error) {
	if zipFileName != "" {
		reader, err := zip.OpenReader(zipFileName)
		if err != nil {
			return nil, err
		}
		for _, file := range reader.File {
			if !file.FileInfo().IsDir() {
				err = addToFileMap(fileMap, file)
				if err != nil {
					return nil, err
				}
			}
		}
		return reader, nil
	}
	return nil, nil
}

func addToFileMap(fileMap map[int]map[string]FileProvider, file *zip.File) error {
	parts := strings.Split(file.Name, "/")
	l := len(parts)
	if l >= 2 {
		folder := parts[l-2]
		fileName := parts[l-1]
		date, err := strconv.Atoi(folder)
		if err != nil {
			return err
		}
		files, ok := fileMap[date]
		if !ok {
			fileMap[date] = map[string]FileProvider{fileName: &zipFileProvider{fileName: fileName, file: file}}
		} else {
			_, ok = files[fileName]
			if !ok {
				files[fileName] = &zipFileProvider{fileName: fileName, file: file}
			}
		}
	}
	return nil
}
