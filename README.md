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
* Implement BFS Direction Optimising.
* Complete implementation for thread pooling to support generic parallelism (basic openmp for mac support).
* Implement individual vertex locking.
* Implement trees for appending edges.
* Implement running track of out degrees for vertices.
* Implement basic environment for random alterations to graph.
* Testing.
