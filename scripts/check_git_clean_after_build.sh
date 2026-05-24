#!/usr/bin/env bash

if [[ "$(git status -s)" == "" ]];
then
    echo -e "\033[1;32mGOOD:\033[0;32m git files are unchanged after build.\033[0m"
else
    echo -e "\033[1;31mERROR:\033[0;31m git files have changed.\033[0m"
    git status
    exit 1
fi
