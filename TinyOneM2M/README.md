# Intro 

make
./server.o

MacOS:

// Já tem o SQLite3 previamente instalado (Mac W)
brew install cJSON

To run valgrind:
```bash
docker pull karek/valgrind:latest
docker run --rm -t -p 8000:8000 -v "$PWD":/valgrind karek/valgrind:latest valgrind
valgrind ./server.o
```

Linux: 
```
//sudo apt install sqlite3 // serviço de sqlite
sudo apt -y install libsqlite3-dev // Biblioteca de sqlite
sudo apt-get install sqlite3 libsqlite3-dev // Ou corres este comando se os de cima não funcionarem
sudo apt-get install libcjson-dev // Não é precisa
```
# Postman

https://app.getpostman.com/join-team?invite_code=d8f9c698034b451b1b2bb25cced71575&target_code=d1daf40b754bb8facf4ec226eeca1dae