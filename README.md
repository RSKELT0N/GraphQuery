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
cmake --build [build_directory] -j20
```
### Clean project
```
rm -rf [build_directory]
```

## Execute GraphQuery
```
./[build_directory]/graph-query/core/graphquery
```

## Algorithms Implemented
- Incremental PageRank
- Weakly Connected Components by label propagation
- Strongly Connected Components, Tarjan's Algorithms
- Direction Optimising Breadth-First Search

## LDBC SNB Queries
- 2 Complex queries
- 2 Short queries
- 2 Delete queries
- 2 Update queries

### On Startup
<img width="1278" alt="Screenshot 2024-04-15 at 15 13 25" src="https://github.com/RSKELT0N/GraphQuery/assets/64985419/7cd70a4c-6161-4281-b213-005c28f0da11">

### Loading a graph
<img width="1276" alt="image" src="https://github.com/RSKELT0N/GraphQuery/assets/64985419/e2d494ba-6c39-4cee-9e19-97257f8afdca">

## Loading dataset and performing workload

Analytic            |  Transactional
:-------------------------:|:-------------------------:
<img width="1279" alt="image" src="https://github.com/RSKELT0N/GraphQuery/assets/64985419/db5c730e-b7eb-4bca-a6dd-cc7458a75b3b"> | <img width="1278" alt="image" src="https://github.com/RSKELT0N/GraphQuery/assets/64985419/5fc1aa29-df34-4574-9783-28e2330ddd27">




