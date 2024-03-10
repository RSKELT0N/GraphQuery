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
* Implement individual vertex locking.
* Implement top roots for edge labels.
* Implement lock free linked list.
* Implement basic environment for random alterations to graph.
* Implement analytic after querying.
* Testing.
