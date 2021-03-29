/**
    Copyright (c) 2021 Ben Cerjan
    This file is part of LoopHeroOptimizer.
    LoopHeroOptimizer is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    LoopHeroOptimizer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.
    You should have received a copy of the GNU Affero General Public License
    along with LoopHeroOptimizer.  If not, see <https://www.gnu.org/licenses/>.
**/

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
#include <string.h>

#define MAX_ROWS 20
#define MAX_COLS 20

// For overall what is in a tile
enum Terrain {LHO_EMPTY = -1, LHO_RIVER = 0, LHO_LANDSCAPE = 1};

// For what specific landscape tile we're using
enum Landscape {LHO_MEADOW = 0, LHO_THICKET = 1, LHO_MOUNTAIN = 2,
                LHO_SUBURB = 3};

// Values for each type of landscape tile
enum Values {LHO_MEADOWVAL = 3, LHO_THICKETVAL = 2, LHO_MOUNTAINVAL = 6,
             LHO_SUBURBVAL = 1};

enum ZigZag {LHO_UP, LHO_DOWN, LHO_LEFT, LHO_RIGHT};


// Struct to hold locations and linear index of the "head" of the river
struct River {
  int headLoc;
  int oldHeadLoc;
  bool newRiver;
};

struct Tile {
  enum Terrain type; // -1 is empty, 0 is river, 1 is landscape
  int numAdjRivers;
  int numAdjLands;
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

static enum Landscape landChoice;
static int landValue; // value for a single landscape tile
static int maxTileVal;

static int bestVal = -1;

// Function to set static land properties:
void init_landscape(int choice)
{
  switch (choice) {
    case 0: // Meadow
      landChoice = LHO_MEADOW;
      landValue = LHO_MEADOWVAL;
      maxTileVal = 3*LHO_MEADOWVAL;
      break;
    case 1: // Thicket
      landChoice = LHO_THICKET;
      landValue = LHO_THICKETVAL;
      maxTileVal = 3*LHO_THICKETVAL;
      break;
    case 2: // Mountain
      landChoice = LHO_MOUNTAIN;
      landValue = LHO_MOUNTAINVAL;
      maxTileVal = 4*LHO_MOUNTAINVAL;
      break;
    case 3: // Suburb
      landChoice = LHO_SUBURB;
      landValue = LHO_SUBURBVAL;
      maxTileVal = 3*LHO_SUBURBVAL;
      break;
  }
}

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
    grid->grid[i] = malloc(numCols * sizeof(struct Tile));
    for (j = 0; j < numCols; j ++) {
      grid->grid[i][j].type = LHO_EMPTY;
      grid->grid[i][j].numAdjRivers = 0;
      grid->grid[i][j].numAdjLands = 0;
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

    // Increase nearby land counts::
    int i = idx[0], j = idx[1];
    // Increment river adjacency counts:
    if (i > 0) {
      grid->grid[i-1][j].numAdjLands++;
    }

    if (i < numRows - 1) {
      grid->grid[i+1][j].numAdjLands++;
    }
    if (j > 0) {
      grid->grid[i][j-1].numAdjLands++;
    }

    if (j < numCols - 1) {
      grid->grid[i][j+1].numAdjLands++;
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

    int i = idx[0], j = idx[1];
    // Increment river adjacency counts:
    if (i > 0) {
      grid->grid[i-1][j].numAdjRivers++;
    }

    if (i < numRows - 1) {
      grid->grid[i+1][j].numAdjRivers++;
    }
    if (j > 0) {
      grid->grid[i][j-1].numAdjRivers++;
    }

    if (j < numCols - 1) {
      grid->grid[i][j+1].numAdjRivers++;
    }


    return true;
  }

  return false; // Shouldn't get here, but just in case
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
  if (oldType > LHO_EMPTY) {
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

  // if we removed a river, decrement river counts:
  if (oldType == LHO_RIVER) {
    int i = idx[0], j = idx[1];
    if (i > 0) {
      grid->grid[i-1][j].numAdjRivers--;
    }

    if (i < numRows - 1) {
      grid->grid[i+1][j].numAdjRivers--;
    }
    if (j > 0) {
      grid->grid[i][j-1].numAdjRivers--;
    }

    if (j < numCols - 1) {
      grid->grid[i][j+1].numAdjRivers--;
    }
  } else if (oldType == LHO_LANDSCAPE) {
    // Increase nearby land counts:
    int i = idx[0], j = idx[1];
    if (i > 0) {
      grid->grid[i-1][j].numAdjLands--;
    }

    if (i < numRows - 1) {
      grid->grid[i+1][j].numAdjLands--;
    }
    if (j > 0) {
      grid->grid[i][j-1].numAdjLands--;
    }

    if (j < numCols - 1) {
      grid->grid[i][j+1].numAdjLands--;
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
        numRivers = grid.grid[i][j].numAdjRivers;
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
        numRivers = grid.grid[i][j].numAdjRivers;
        numSuburbs = grid.grid[i][j].numAdjLands;
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
        numRivers = grid.grid[i][j].numAdjRivers;
        numMountains = grid.grid[i][j].numAdjLands;
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
  char label[4];

  // Speficy landscape string to match which type of thing we optimized:
  switch (landChoice) {
    case LHO_MEADOW:
    case LHO_MOUNTAIN:
      strcpy(label, " M ");
      break;
    case LHO_SUBURB:
      strcpy(label, " S ");
      break;
    case LHO_THICKET:
      strcpy(label, " T ");
      break;
  }

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
          printf("%s", label);
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

/*
  Sets initial grid used in recursion using some heuristics to start at a
  higher "current best"
  Starts river at top left and tries to draw a zig-zag to the bottom then back
  up, repeating if necessary. The rest is filled with landscape tiles, minus
  one tile to enter recursion once
*/

void heuristic_grid(struct Grid *grid)
{
  int i,j;


  for (i = 0; i < numRows; i++) {
    for (j = 0; j < numCols; j++) {
      grid->grid[i][j].type = LHO_LANDSCAPE; // Set whole grid to landscape
    }
  }

  i = 0;
  j = 0;
  bool done = false;
  bool firstLoop = true;
  int zigx = 0, zigy = 0;
  enum ZigZag zigLocalDir = LHO_RIGHT; // always start top left, so first move is to the right
  enum ZigZag zigNextDir = LHO_DOWN; // start top left and overall go down to the right
  enum ZigZag zigOverallDir = LHO_DOWN;

  while (!done) {

    grid->grid[i][j].type = LHO_RIVER;

    if (j == numCols - 1) {
      done = true;
    }

    switch (zigLocalDir) {
      case LHO_RIGHT:
        j++;
        zigLocalDir = zigNextDir;
        if (zigOverallDir == LHO_DOWN) {
          zigNextDir = LHO_DOWN;
        } else {
          zigNextDir = LHO_UP;
        }
        break;
      case LHO_LEFT:
        j--;
        zigLocalDir = zigNextDir;
        if (zigOverallDir == LHO_DOWN) {
          zigNextDir = LHO_DOWN;
        } else {
          zigNextDir = LHO_UP;
        }
        break;
      case LHO_DOWN: // these are "backwards" as top left is (0,0)
        i++;
        zigLocalDir = LHO_RIGHT;
        break;
      case LHO_UP:
        i--;
        zigLocalDir = LHO_RIGHT;
        break;
    }

    if (i == (numRows)) {
      zigOverallDir = LHO_UP;
      //zigLocalDir = LHO_RIGHT;
      zigNextDir = LHO_UP;
      i -= 2;
      //j++;
    }

    if (i == -1 && !firstLoop) {
      zigOverallDir = LHO_DOWN;
      //zigLocalDir = LHO_RIGHT;
      zigNextDir = LHO_DOWN;
      i += 2;
      //j++;
    }

    firstLoop = false;

  }

  return;
}


static int recursion_depth = 0;
static bool initial_recursion = true;

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

  if (initial_recursion) {
    heuristic_grid(&tempGrid);
    currentBest = val_calc(tempGrid);
    bestVal =  currentBest;
    copy_grid(&bestGrid, &tempGrid);
    initial_recursion = false;
  }

  for (i = 0; i < maxLen; i++) {
      for (j = 0; j < 2; j++) {
        if (j == 0) {
          bool river_added = add_river(i, &thisGrid);
          //printf("added river at i: %d\n", i);
          if (river_added) {
            //printf("Actually added river...\n");
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
  int land;

  // Get input for optimization
  printf(" Enter information about the grid to optimize...\n\n How many rows?\n  ");
  scanf("%d", &rows);
  printf(" How many columns?\n  ");
  scanf("%d", &cols);
  printf(" What type of landscape tile?\n (0 = meadow, 1 = thicket, 2 = mountain, 3 = suburb):\n  ");
  scanf("%d", &land);

  init_landscape(land);


  numRows = rows;
  numCols = cols;


  // allocate memory for our grid
  struct Grid grid;
  allocate_grid(&grid);

  printf("\n starting recursion...\n");
  recurse_grid(&grid);
  print_grid(grid);

  int val;
  val = val_calc(grid);
  printf(" Value of grid: %d\n", val);

  free_grid(&grid);
  return 0;

}
