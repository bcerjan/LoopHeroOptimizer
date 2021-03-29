# LoopHeroOptimizer
Simple C code to perform optimization for placement of landscape tiles and rivers in the game [Loop Hero](https://www.loophero.com/).
Uses the command line to specify what grid size and what landscape tile should be optimized. Assumes the 'optimal' form of that landscape tile (e.g. thickets over forests, blooming meadow over normal meadow,...).

Note that for larger grids (above ~3x5) it can take a *very* long time to run. On my computer (Intel i5) a 3x5 grid takes ~30 minutes.
