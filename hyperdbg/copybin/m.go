package main

import (
	"github.com/ddkwork/golibrary/std/stream"
	"io/fs"
	"path/filepath"
	"strings"
)

func main() {
	root := "../cmake-build-release"
	cliDir := filepath.Join(root, "hyperdbg-cli")
	filepath.Walk(root, func(path string, info fs.FileInfo, err error) error {
		switch filepath.Ext(path) {
		case ".dll", ".sys", ".exe":
			if strings.HasPrefix(filepath.Base(path), "CMake") {
				return err
			}
			println(path)
			stream.CopyFile(path, filepath.Join(cliDir, filepath.Base(path)))
		}
		return err
	})

	//filepath.Glob(root)
}
