# GraphQuery #
*A graph database following the labelled property graph (LPG) model, specifically loading the LDBC SNB dataset. Rendering through a GUI application to query through read/write transactions and analytic workloads..*

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
* Implement basic environment for random alterations to graph.
* Testing.
