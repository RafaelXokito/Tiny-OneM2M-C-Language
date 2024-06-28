#!/bin/bash
docker run --rm -t -p 8000:8000 -v "$PWD":/valgrind karek/valgrind:latest valgrind "$@"
