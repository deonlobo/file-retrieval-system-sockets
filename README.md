﻿# Distributed File Retrieval System

## Project Overview

- Client-server project enabling file requests.
- Clients connect to server and request files.
- Server searches for files and returns them or appropriate message.
- Returns a compressed tar archive containing all files found based on the command.
- Supports multiple clients from different machines.

## Server (serverw24)

- Runs alongside mirror1 and mirror2.
- Forks a child process to handle each client request.
- Uses a `crequest()` function to process client commands.
- Exits upon receiving `quitc` command.

## Client (clientw24)

- Runs an infinite loop waiting for user commands.
- Verifies command syntax before sending to server.
- Commands include `dirlist`, `w24fn`, `w24fz`, `w24ft`, `w24fdb`, `w24fda`, `quitc`.

## Command List

- `dirlist -a`: Returns subdirectories alphabetically.
- `dirlist -t`: Returns subdirectories by creation date.
- `w24fn filename`: Retrieves file info or prints "File not found".
- `w24fz size1 size2`: Returns compressed tar of all files within size range.
- `w24ft <extension list>`: Returns compressed tar of all files with specified extensions.
- `w24fdb date`: Returns compressed tar of all files created before specified date.
- `w24fda date`: Returns compressed tar of all files created after specified date.
- `quitc`: Terminates client process.

## Alternating Servers (serverw24, mirror1, mirror2)

- Each handles a set number of client connections in sequence.
- Initial connections handled by serverw24, then alternating between servers.
- Ensures load balancing across servers.
