# qstat
QStat is a command-line program that displays information about Internet game servers. The servers are either down, non-responsive, or running a game. For servers running a game, the server name, map name, current number of players, and response time are displayed. Server rules and player information may also be displayed.

## Supported Games
Games supported include Quake, QuakeWorld, Hexen II, Quake II, HexenWorld, Unreal, Half-Life, Sin, Shogo, Tribes, Tribes 2, Quake III: Arena, BFRIS, Kingpin, and Heretic II, Unreal Tournament, Soldier of Fortune, Rogue Spear, Redline, Turok II, Blood 2, Descent 3, Drakan, KISS, Nerf Arena Blast, Rally Master, Terminous, Wheel of Time, and Daikatana and many more. Note for Tribes 2: QStat only supports Tribes 2 builds numbered 22075 or higher. Note for Ghost Recon QStat only supports GhostRecon patch 1.2, 1.3, 1.4, Desert Siege, and Island Thunder.

This list is currently outdated, for a full list see the output from qstat itself.

The different server types can be queried simultaneously. If qStat detects that this is being done, the output is keyed by the type of server being displayed. See Display Options.

The game server may be specified as an IP address or a hostname. Servers can be listed on the command-line or, with the use of the -f option, a text file.

## Display Options
One line will be displayed for each server queried. The first component of the line will be the server's address as given on the command-line or the file. This can be used as a key to match input addresses to server status. Server rules and player information are displayed under the server info, indented by one tab stop.

qstat supports three additional display modes: raw, templates, and XML. In raw mode, the server information is displayed using simple delimiters and no formatting. This mode is good for programs that parse and reformat QStat's output. The template mode uses text files to layout the server information within existing text. This is ideal for generating web pages. The XML mode outputs server information wrapped in simple XML tags. The raw mode is enabled using the -raw option, template output is enabled using -Ts, and XML output is enabled with -xml.

## Game Options
These options select which servers to query and what game type they are running. Servers are specified by IP address (for example: 199.2.18.4) or hostname. Servers can be listed on the command-line or in a file (see option -f.) The game type of a server can be specified with its address, or a default game type can be set for all addresses that don't have a game type.

The following table shows the command-line option and type strings for the supported game types. The type string is used with the -default option and in files with the -f option.

## Configuration files
The games supported by qstat can be customized with configuration files. The query parameters of built-in game types can be modified and new games can be defined.
For built-in game types, certain parameters can be modified. The parameters are limited to the master server protocol and master server query string.

New game types can be defined as a variation on an existing game type. Most new games use a Quake 3 or Gamespy/Unreal based network engine. These games can already be queried using -q3s or -gps, but they don't have game specific details such as the correct default port, the game name, and the correct "game" or "mod" server rule. And, mostly importantly, they don't get their own game type string (e.g. q3s, rws, t2s). All of these details can be specified in the QStat config file.

qstat comes with a default configuration file called 'qstat.cfg'. If this file is found in the directory where qstat is run, the file will be loaded. Configuration files can also be specified with the QSTAT_CONFIG environment variable and the -cfg command-line option. See Appendix B for a description of the configuration file format.

## Migration to Github
I've been the only active maintainer for the old [qstat sourceforge repo](http://sourceforge.net/projects/qstat/) for a while now. Unfortunately I don't have full admin their and have been unable to contact the original qstat project creator [Steve Jankowski](https://sourceforge.net/u/stevejankowski/profile/) for some time, so I've decided to move the project to github.

Hence for all intensive purposes https://github.com/multiplay/qstat is the new official home of the qstat repro.
