package main

import (
	"github.com/ddkwork/golibrary/std/mylog"
	"github.com/ddkwork/golibrary/std/stream"
	"io/fs"
	"path/filepath"
	"strings"
)

func main() {
	root := "../cmake-build-release"
	cliDir := filepath.Join(root, "debugger_server")
	var bins []string
	filepath.Walk(root, func(path string, info fs.FileInfo, err error) error {
		if strings.Contains(path, "debugger_server") {
			return err
		}
		switch filepath.Ext(path) {
		case ".dll", ".sys", ".exe":
			if strings.HasPrefix(filepath.Base(path), "CMake") {
				return err
			}
			if strings.HasPrefix(filepath.Base(path), "Compiler") {
				return err
			}
			bins = append(bins, path)
		}
		return err
	})
	mylog.Struct(bins)

	targetDir := ""
	filepath.Walk(cliDir, func(path string, info fs.FileInfo, err error) error {
		if filepath.Base(path) == "debugger_server.exe" {
			targetDir = filepath.Dir(path)
			return err
		}
		return err
	})

	for _, bin := range bins {
		stream.CopyFile(bin, filepath.Join(targetDir, filepath.Base(bin)))
	}
}
