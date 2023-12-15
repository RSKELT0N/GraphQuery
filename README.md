# GraphQuery #
*A graph database to represent edges between nodes
that hold properties and information rendered through a GUI application..*

## Build and compile project


### Build project
```
cmake --list-presets                            ; View available presets
cmake --preset [preset] -B [build_directory]    ; Build project with defined preset
```
### Compile
```
cmake --build [build_directory]
```
### Clean project
```
rm -rf [build_directory]
```

## TODO
* Comment codebase
* Structure core subdirectory
* Complete gui output class.
* Complete menu bar (reading file of certain format).
* Start common API for storage methods.
* Start common API for graph analytics for storage methods (edges, all vertices, connections).
* Move log system to shared lib.
* Build CYPHER language ontop of system.
* Build algorithms ontop of systems, interacting with CYPHER.
* Display nodes in visualiser.
* Create analytics frame.
