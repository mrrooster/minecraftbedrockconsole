
# Minecraft Bedrock Server Console

This provides a GUI console window for the experimental [Minecraft Bedrock edition server](https://www.minecraft.net/en-us/download/server/bedrock/) from Microsoft.

![The main window](doc/img/main_server.png)

It started because I wanted a nice easy way of taking a backup from a running server.

To use it, install it and point it at your minecraft server.

Also, create a folder for backups, and tell it about that too.

**IMPORTANT:** There is no guarantee offered with this software. Make sure you have full backups before using it. There is no warrenty AT ALL.

## Features

* Start/stop your server easly and safely.
* View the servers log output
* A console for sending commands to the server
* Take a backup without stopping the server
* Backups can be automatic, or on certain events.

## Options

![The options window](doc/img/main_options.png)

## Backups

Backups are taken while the server is running. This is done in co-operation with the server and is perfectly safe. As well as your world files the backup contains some of the server settings files too.

### Restoring backups

**IMPORTANT**: The backup zip file contains a `worlds` folder, which contains the world that was running on the server when the backup was created. If you want to restore a backup it is not sufficient to copy this back over the folder on the server.

To restore a backup:

1. Shut down your server
2. In the server folder, delete or rename the `worlds` folder. If you have multiple worlds you should take a backup of this folder.
3. Copy the `worlds` folder from the backup zip to the server folder.
4. Start the server.

**You must not copy the `worlds` folder over an existing `worlds` folder as this does not seem to work correctly.**
