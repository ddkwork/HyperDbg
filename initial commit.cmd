rmdir /s .git
git init
git add .
git commit -m "initial commit"
git remote add origin https://github.com/ddkwork/HyperDbg.git
git remote set-url origin https://github.com/ddkwork/HyperDbg.git
::git remote set-url origin https://ddkwork:your_tk_here@github.com/ddkwork/HyperDbg.git
git push -u -v --force origin master
