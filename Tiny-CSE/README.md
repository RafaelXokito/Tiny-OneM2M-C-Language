# Intro 

cd Tiny-CSE
make
./server.o

MacOS:

// Já tem o SQLite3 previamente instalado (Mac W)
brew install cJSON

Windows: 

https://www.bing.com/videos/search?q=sqlite3%27+is+not+recognized+as+an+internal+or+external+command&&view=detail&mid=DBE62008D84D0B309C10DBE62008D84D0B309C10&&FORM=VRDGAR&ru=%2Fvideos%2Fsearch%3Fq%3Dsqlite3%2527%2Bis%2Bnot%2Brecognized%2Bas%2Ban%2Binternal%2Bor%2Bexternal%2Bcommand%26FORM%3DHDRSC4

On Windows, you can download the pre-built library files from the cJSON GitHub repository and include them in your project. Here are the steps to do so:

Go to the cJSON GitHub repository at https://github.com/DaveGamble/cJSON.

Download the latest release from the "Releases" section.

Extract the downloaded archive and copy the cJSON.h header file and the cJSON.lib or cJSON.dll library files into your project directory.

In your project, include the cJSON.h header file and link against the cJSON.lib or cJSON.dll library file.

Note that the library file you choose (cJSON.lib or cJSON.dll) will depend on whether you want to statically or dynamically link against the library.

Linux: 
```
//sudo apt install sqlite3
sudo apt -y install libsqlite3-dev
sudo apt-get install libcjson-dev
```
# Postman

https://app.getpostman.com/join-team?invite_code=d8f9c698034b451b1b2bb25cced71575&target_code=d1daf40b754bb8facf4ec226eeca1dae