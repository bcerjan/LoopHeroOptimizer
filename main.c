// Code to simulate optimal placement of rivers and landscape tiles in Loop Hero
// Designed to work for meadows (assumes blooming), forest/thicket (assumes
// thickets), rocks/mountains (assumes mountains), and suburbs.

// Assume the hgiher-valued ones, because if you're going through the trouble
// to optimize like this, you might as well also select for the best tiles

/*
  Uses linear indexing of the following format to address the tile grid:
   0 | 1 | 2 | 3
   4 | 5 | 6 | 7
   8 | 9 | 10 | 11 ...
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_ROWS 20
#define MAX_COLS 20
// For overall what is in a tile
enum Terrain {LHO_EMPTY = -1, LHO_RIVER = 0, LHO_LANDSCAPE = 1};

// For what specific landscape tile we're using
enum Landscape {LHO_MEADOW = 0, LHO_THICKET = 1, LHO_MOUNTAIN = 2,
                LHO_SUBURB = 3};

// Values for each type of landscape tile
enum Values {LHO_MEADOWVAL = 3, LHO_THICKETVAL = 2, LHO_MOUNTAINVAL = 5,
             LHO_SUBURBVAL = 1};

// Struct to hold locations and linear index of the "head" of the river
struct River {
  int headLoc;
  int oldHeadLoc;
  bool newRiver;
};

struct Tile {
  enum Terrain type; // -1 is empty, 0 is river, 1 is landscape
};

struct Grid {
  struct Tile **grid;
  struct River river;
  int numFilledTiles;
  int maxTiles;
  bool full;
  int val;
};

static int numRows;
static int numCols;

static enum Landscape landChoice; // 0 = meadow, 1 = thicket, 2 = mountain, 3 = suburb
static int landValue; // value for a single landscape tile
static int maxTileVal;

static int bestVal = -1;

// function to return row for a given linear index
int get_row_idx(int linIndex)
{
  return linIndex / numCols;
}

// function to return col for a given linear index
int get_col_idx(int linIndex)
{
  return linIndex % numCols;
}

// returns array of size 2 with idx[0] = row and idx[1] = col
void get_idx(int linIndex, int idx[2]) // always of length 2
{
  idx[0] = get_row_idx(linIndex);
  idx[1] = get_col_idx(linIndex);

  //printf("row: %d, col: %d\n", idx[0], idx[1]);

  return;
}

// Function to allocate a grid, defaults to empty cells:
void allocate_grid(struct Grid *grid)
{
  int i,j;
  grid->grid = malloc(numRows * sizeof(grid->grid));
  for (i = 0; i < numRows; i++) {
    grid->grid[i] = malloc(numCols * sizeof(grid->grid));
    for (j = 0; j < numCols; j ++) {
      grid->grid[i][j].type = LHO_EMPTY;
    }
  }

  grid->river.newRiver = true;
  grid->river.headLoc = -1;
  grid->river.oldHeadLoc = -1;
  grid->full = false;
  grid->numFilledTiles = 0;
  grid->maxTiles = numRows * numCols;
  grid->val = -1;

  return;
}

// de-allocate our array
void free_grid(struct Grid *grid)
{
  int i;
  for (i = 0; i < numRows; i++) {
    free(grid->grid[i]);
  }
  free(grid->grid);
  return;
}

// Performs deep copy of grid from one pointer to another
// copies from inGrid to dupGrid, both need to be freed separately.
void copy_grid(struct Grid *dupGrid, struct Grid *inGrid)
{
  dupGrid->numFilledTiles = inGrid->numFilledTiles;
  dupGrid->maxTiles = inGrid->maxTiles;
  dupGrid->full = inGrid->full;
  dupGrid->val = inGrid->val;
  dupGrid->river = inGrid->river;

  int i,j;

  for (i = 0; i < numRows; i++) {
    for (j = 0; j < numCols; j++) {
      dupGrid->grid[i][j] = inGrid->grid[i][j];
    }
  }
}


// Function that returns the linear indices of adjacent tiles to the current one
// Negative return indicates outside the matrix
// Does not check for filled status of neighbors
void get_adj_locs(int linIndex, int loc[4]) // always of length 4
{
  int i;

  loc[0] = linIndex + 1; // Right
  loc[1] = linIndex - 1; // Left
  loc[2] = linIndex - numCols; // Up
  loc[3] = linIndex + numCols; // Down

  for (i = 0; i < 4; i++) {
    if (loc[i] < 0 || loc[i] > numRows * numCols - 1) {
      loc[i] = -1; // outside the grid
    }
  }

  return;
}

// Counts number of adjacent rivers for calculating score
// as this is designed to be used with a final grid, takes row, col inputs
// rather than linear index
int count_adj_rivers(int i, int j, struct Grid grid)
{
  int count = 0;

  // If's are to prevent looking outside our matrix
  if (i > 0) {
    if (grid.grid[i-1][j].type == LHO_RIVER) { count++; }
  }

  if (i < numRows - 1) {
    if (grid.grid[i+1][j].type == LHO_RIVER) { count++; }
  }

  if (j > 0) {
    if (grid.grid[i][j-1].type == LHO_RIVER) { count++; }
  }

  if (j < numCols - 1) {
    if (grid.grid[i][j+1].type == LHO_RIVER) { count++; }
  }


  return count;
}

// Counts number of adjacent landscapes for calculating score
// as this is designed to be used with a final grid, takes row, col inputs
// rather than linear index
int count_adj_lands(int i, int j, struct Grid grid)
{
  int count = 0;

  if (i > 0) {
    if (grid.grid[i-1][j].type == LHO_LANDSCAPE) { count++; }
  }

  if (i < numRows - 1) {
    if (grid.grid[i+1][j].type == LHO_LANDSCAPE) { count++; }
  }

  if (j > 0) {
    if (grid.grid[i][j-1].type == LHO_LANDSCAPE) { count++; }
  }

  if (j < numCols - 1) {
    if (grid.grid[i][j+1].type == LHO_LANDSCAPE) { count++; }
  }

  return count;
}


// Because Mountains count diagonal as adjacent (FOR OTHER MOUNTAINS, not rivers)
// they get their own counting function
int count_adj_mountains(int i, int j, struct Grid grid)
{
  int count = 0;

  // we're not on an edge:
  if ( (i > 0) && (i < numRows - 1) && (j > 0) && (j < numCols - 1) ) {
    // Cardinal Diretions:
    if (grid.grid[i-1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i][j+1].type == LHO_LANDSCAPE) { count++; }
    // Diagonals:
    if (grid.grid[i-1][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j+1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j+1].type == LHO_LANDSCAPE) { count++; }
  }
  if (i == 0 && j > 0 && j < numCols - 1) { // left edge, no corners
    if (grid.grid[i+1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j+1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j-1].type == LHO_LANDSCAPE) { count++; }
  }

  if ( (i == numRows - 1) && j > 0 && j < numCols - 1) { // right edge, no corners
    if (grid.grid[i-1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j+1].type == LHO_LANDSCAPE) { count++; }
  }

  if (j == 0 && i > 0 && i < numRows - 1) { // Bottom, no corners
    if (grid.grid[i][j+1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j+1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j+1].type == LHO_LANDSCAPE) { count++; }
  }

  if ( (j == numCols - 1) && i > 0 && i < numRows - 1) { // top, no corners
    if (grid.grid[i][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j-1].type == LHO_LANDSCAPE) { count++; }
  }

  // Now do corners:
  if ( i == 0 && j == 0) { // top left
    if (grid.grid[i][j+1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j+1].type == LHO_LANDSCAPE) { count++; }
  }

  if ( i == 0 && (j == numCols - 1)) { // top right
    if (grid.grid[i][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i+1][j-1].type == LHO_LANDSCAPE) { count++; }
  }

  if ( (i == numRows - 1) && j == 0) { // bot left
    if (grid.grid[i][j+1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j+1].type == LHO_LANDSCAPE) { count++; }
  }

  if ( (i == numRows - 1) && (j == numCols - 1) ) { // bot right
    if (grid.grid[i][j-1].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j].type == LHO_LANDSCAPE) { count++; }
    if (grid.grid[i-1][j-1].type == LHO_LANDSCAPE) { count++; }
  }

  return count;
}


// Function to check if a location is already occupied
// returns false if occupied, true if unoccupied
bool chk_loc(int linIndex, struct Grid grid)
{
  if (linIndex < 0 || linIndex > numRows * numCols - 1) {
    //printf("Outside Array\n");
    return false; // we can't write outside our array...
  }

  int idx[2] = {0};
  get_idx(linIndex, idx);
  if ( grid.grid[idx[0]][idx[1]].type > LHO_EMPTY ) {
    // Location in use
    //printf("Bad Location\n");
    return false;
  }
    //printf("Good Location\n");
  return true;
}

// Function to get valid river locations (in linear index):
// returns list of indices -- negative numbers indicate a given location is
// occupied already by something
void valid_river_locs(struct Grid grid, int locs[4]) // alawys of length 4
{
  get_adj_locs(grid.river.headLoc, locs); // one is always occupied by previous head
  int i;

  for (i = 0; i < 4; i++) {
    if ( !chk_loc(locs[i], grid) ) {
      locs[i] = -1; // this space is occupied
    }
  }

  return;
}

// Function to add a landscape tile at a given linear index
// if index is < 0, we want a random location for the first tile
// designed to be used like: add_land(index, &grid); to update in place
// returns true when land was added, false if not
bool add_land(int linIndex, struct Grid *grid)
{

  //struct Grid g = *grid;
  bool validLoc = chk_loc(linIndex, *grid);

  if (validLoc) {
    int idx[2] = {0};
    get_idx(linIndex, idx);
    grid->grid[idx[0]][idx[1]].type = LHO_LANDSCAPE;
    grid->numFilledTiles++;
    if (grid->numFilledTiles == grid->maxTiles) {
      grid->full = true;
    }
    return true;
  }

  return false;
}

// designed to be used like: add_river(index, &grid); to update in place
// returns true on successful add, false on failure
bool add_river(int linIndex, struct Grid *grid)
{
  int curLoc;
  int idx[2] = {0};
  get_idx(linIndex, idx);

  if (grid->river.newRiver) {
    // Note that we have to start on a border
    if (idx[0] == 0 || idx[1] == 0 ||
        idx[0] == numRows - 1 || idx[1] == numCols - 1) {
      grid->river.newRiver = false;
    } else {
      //printf("Not starting on edge!\n");
      return false;
    }
  }

  //struct Grid g = *grid;
  bool validLoc = chk_loc(linIndex, *grid);

  curLoc = grid->river.headLoc;
//printf("curLoc: %d\n", curLoc);
  int curIdx[2];
  get_idx(curLoc, curIdx);
//printf("idx[0]: %d, idx[1]: %d\ncurIdx[0]: %d, curIdx[1]: %d\n", idx[0], idx[1], curIdx[0], curIdx[1]);
  if (
    !(
      ( (abs(idx[0] - curIdx[0]) == 1) && (abs(idx[1] - curIdx[1]) == 0) ) ||
      ( (abs(idx[0] - curIdx[0]) == 0) && (abs(idx[1] - curIdx[1]) == 1) )
    ) && curLoc > -1)
  {
    //printf("Not near previous river head!\n");
    return false; // River has to connect to previous segments of river
  }

  if (validLoc) {
    grid->river.oldHeadLoc = grid->river.headLoc;
    grid->river.headLoc = linIndex;

    grid->grid[idx[0]][idx[1]].type = LHO_RIVER;
    grid->numFilledTiles++;
    if (grid->numFilledTiles == grid->maxTiles) {
      grid->full = true;
    }

  }

  return true;
}


// function to remove terrain at a given location, sets to empty
void remove_terrain(int linIndex, struct Grid *grid)
{
  int idx[2];
  int headLoc = grid->river.headLoc;
  enum Terrain oldType;

  get_idx(linIndex, idx);
  oldType = grid->grid[idx[0]][idx[1]].type;
  grid->grid[idx[0]][idx[1]].type = LHO_EMPTY;
  if (oldType > -1) {
    grid->numFilledTiles--; // decrement counter of filled tiles
  }

  // If we move the head of the river, we need to update to reflect that
  // potentially also allwoing a new river to start if we removed all of it.
  if (headLoc == linIndex) {
    grid->river.headLoc = grid->river.oldHeadLoc;
    if (grid->river.headLoc == -1) {
      grid->river.newRiver = true;
    }
  }

  return;
}

// for meadows and thickets:
int val_calc_meadow_thicket(struct Grid grid)
{
  int i,j;
  int val = 0;
  int numRivers;

  for (i = 0; i < numRows; i++) {
    for (j = 0; j < numCols; j++) {
      if (grid.grid[i][j].type == LHO_LANDSCAPE) {
        numRivers = count_adj_rivers(i, j, grid);
        if (numRivers == 0) {
          val += landValue;
        } else {
          val += (landValue * 2) * numRivers; // might be a more clever way to do this besides if/else
        }
      }
    }
  }

  return val;
}

// Function for calculating value of a suburb
int val_calc_suburb(struct Grid grid)
{
  int i,j;
  int val = 0;
  int numRivers, numSuburbs;

  for (i = 0; i < numRows; i++) {
    for (j = 0; j < numCols; j++) {
      if (grid.grid[i][j].type == LHO_LANDSCAPE) {
        numRivers = count_adj_rivers(i, j, grid);
        numSuburbs = count_adj_lands(i, j, grid);
        if ( numSuburbs == 4 ) {
          val += 2*landValue;
        } else if (numRivers != 0) {
          val += (landValue * 2) * numRivers;
        } else {
          val += landValue;
        }
      }
    }
  }

  return val;
}

// Function for calculating value of a mountain
int val_calc_mountain(struct Grid grid)
{
  int i,j;
  int val = 0;
  int numRivers, numMountains;

  for (i = 0; i < numRows; i++) {
    for (j = 0; j < numCols; j++) {
      if (grid.grid[i][j].type == LHO_LANDSCAPE) {
        numRivers = count_adj_rivers(i, j, grid);
        numMountains = count_adj_lands(i, j, grid);
        val += numMountains * landValue;
        val += numMountains * numRivers * landValue;
      }
    }
  }

  return val;
}

// Function that returns value of a given grid:
// actually sub-delegates to one of the functions for a particular type of
// landscape tiles
int val_calc(struct Grid grid)
{
  int val;

  switch (landChoice) {
    case LHO_MEADOW:
    case LHO_THICKET:
      val = val_calc_meadow_thicket(grid);
      break;
    case LHO_SUBURB:
      val = val_calc_suburb(grid);
      break;
    case LHO_MOUNTAIN:
      val = val_calc_mountain(grid);
      break;
  }

  return val;
}

// Function to print ASCII structure of a grid
void print_grid(struct Grid grid)
{
  int i,j;
  enum Terrain type;

  printf("\n  ");
  for (i = 0; i < numCols; i++) {
    printf("----");
  }
  printf("-\n");
  for (i = 0; i < numRows; i++) {
    printf("  ");
    for (j = 0; j < numCols; j++) {
      printf("|");
      type = grid.grid[i][j].type;
      switch (type) {
        case LHO_EMPTY:
          printf("   ");
          break;
        case LHO_RIVER:
          printf(" R ");
          break;
        case LHO_LANDSCAPE:
          printf(" L ");
          break;
      } // switch
    } // iForLoop
    printf("|");
    printf("\n  ");
    for (j = 0; j < numCols; j++) {
      printf("----");
    }
    printf("-\n");
  } //jForLoop
  printf("\n");
}

static int recursion_depth = 0;

// Function to fill the remainder of a given grid, designed to be recursed
struct Grid * recurse_grid(struct Grid *grid)
{
//printf("inside recursion, bestVal = %d\n", bestVal);
  //print_grid(*grid);
  // First check if we need to do anything or if grid is full
  if (grid->full) {
    //printf("grid full!\n");
    return grid;
  }

  // Then check if we can exit early due to this branch being unable to surpass
  // this highest value already found:
  int val, maxRemaining;
  val = val_calc(*grid);
  maxRemaining = maxTileVal * (grid->maxTiles - grid->numFilledTiles);
  if ( val + maxRemaining <= bestVal ) {
    //printf("Branch maximum too low to gon on\n");
    return grid;
  }

  int currentBest = bestVal;
  struct Grid bestGrid;
  allocate_grid(&bestGrid);
  copy_grid(&bestGrid, grid);

  int maxLen,i,j;
  maxLen = numRows * numCols;

  struct Grid thisGrid;
  allocate_grid(&thisGrid);
  copy_grid(&thisGrid, grid);

  struct Grid tempGrid;
  allocate_grid(&tempGrid);

  for (i = 0; i < maxLen; i++) {
    if (chk_loc(i, thisGrid)) {
      for (j = 0; j < 2; j++) {
        if (j == 0) {
          bool river_added = add_river(i, &thisGrid);
          if (river_added) {
            copy_grid(&tempGrid, &thisGrid);
            remove_terrain(i, &thisGrid);
            recurse_grid(&tempGrid);
            val = val_calc(tempGrid);
            if (val > currentBest) {
              currentBest = val;
              bestVal = val;
              copy_grid(&bestGrid, &tempGrid);
            }
            recursion_depth--;
          }
        } else {
          bool land_added = add_land(i, &thisGrid);
          if (land_added) {
            copy_grid(&tempGrid, &thisGrid);
            remove_terrain(i, &thisGrid);
            recursion_depth++;
            recurse_grid(&tempGrid);
            val = val_calc(tempGrid);
            if (val > currentBest) {
              currentBest = val;
              bestVal = val;
              copy_grid(&bestGrid, &tempGrid);
            }
            recursion_depth--;
          }
        } /* ifElse */
      } /* jForLoop */
    } /* if chk_loc */
  } /* iForLoop */


  copy_grid(grid, &bestGrid);

  free_grid(&thisGrid);
  free_grid(&bestGrid);
  free_grid(&tempGrid);

  return grid;
}


int main()
{

  int rows;
  int cols;

  // Get these as inputs, rather than defining here
  rows = 3;
  cols = 3;

  numRows = rows;
  numCols = cols;
  landChoice = LHO_MOUNTAIN;
  landValue = LHO_MOUNTAINVAL;
  maxTileVal = landValue * 9;

  // allocate memory for our grid
  struct Grid grid;
  allocate_grid(&grid);

  printf("starting recursion...\n");
  recurse_grid(&grid);
  print_grid(grid);

  int val;
  val = val_calc(grid);
  printf("Value of grid: %d\n", val);

  free_grid(&grid);
  return 0;

}
