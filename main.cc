#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <stack>
#include <unordered_map>
#include <unordered_set>

// clang++ -std=c++20 -O2 main.cc -o bin && ./bin -v

#define W 0x1
#define B 0x2
#define D 0x3

#define FROM_FILE true
#define DUMP_TO_FILE true // Will only dump if FROM_FILE is false

//#define MOVES_TO_DRAW 32
//#define LIMIT_TOTAL_MOVES

#define PRINT_TREE_SIZE_RESOLUTION 15
//#define TREE_SIZE_LIMIT  100000000000
//#define STACK_SIZE_LIMIT 100000000000

//#define DEBUG


struct Board {
  int positions[3][3] = {0};
  int8_t move = 0; // Whose move is that, W/B
  // counts of available (unplayed yet) pieces, big to small
  int white_pieces[3] = {0};
  int black_pieces[3] = {0};
  #ifdef LIMIT_TOTAL_MOVES
  int moves_so_far = 0;
  #endif
};

// Returns piece color at given sub-position. 0 - biggest piece
int sub_pos(int position, int sub_position) {
  int shift = sub_position * 2;
  return (position >> shift) & 0x3;
}

int remove_from_position(int position, int size) {
  int shift = 2*size;
  return position & (~(0x3<<shift));
}

int add_to_position(int position, int size, int color) {
  int shift = 2*size;
  return position | color<<shift;
}

// white piece positions then black pieces, then move.
using CompressedBoard = int64_t;

CompressedBoard Compress(const Board& b) {
  std::map<int, std::vector<std::pair<int, int>>> white_locations;
  std::map<int, std::vector<std::pair<int, int>>> black_locations;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
	int color = sub_pos(b.positions[i][j], k);
	if (color == 0) continue;
	if (color == W) {
	  white_locations[k].push_back(std::make_pair(i,j));
	} else {
	  black_locations[k].push_back(std::make_pair(i,j));
	}
      }
    }
  }
  for (int size = 0 ; size < 3; size++) {
    for (int dummy = 0; dummy < b.white_pieces[size]; dummy++) {
      white_locations[size].push_back(std::make_pair(3,3));
    }
    for (int dummy = 0; dummy < b.black_pieces[size]; dummy++) {
      black_locations[size].push_back(std::make_pair(3,3));
    }
  }

  CompressedBoard out = 0;

  for (int size = 0; size < 3; size++) {
    for (int k = 0; k < 2; k++) {
      auto pair = white_locations.at(size)[k];
      int i = pair.first;
      int j = pair.second;
      out = out * 4 + i;
      out = out * 4 + j;
    }
  }

  for (int size = 0; size < 3; size++) {
    for (int k = 0; k < 2; k++) {
      auto pair = black_locations.at(size)[k];
      int i = pair.first;
      int j = pair.second;
      out = out * 4 + i;
      out = out * 4 + j;
    }
  }

  out = out * 4 + b.move;

  #ifdef LIMIT_TOTAL_MOVES
  out = out * 256 + b.moves_so_far;
  #endif

  return out;
}

Board Decompress(CompressedBoard b) {
  std::map<int, std::vector<std::pair<int, int>>> white_locations;
  std::map<int, std::vector<std::pair<int, int>>> black_locations;

  Board out = {0};

  #ifdef LIMIT_TOTAL_MOVES
  out.moves_so_far = b % 256;
  b = b / 256;
  #endif

  out.move = b % 4;
  b = b / 4;

  for (int size = 2; size >= 0; size--) {
    for (int k = 1; k >=0; k--) {
      int j = b % 4;
      b = b / 4;
      int i = b % 4;
      b = b / 4;
      black_locations[size].push_back(std::make_pair(i,j));
    }
  }

  for (int size = 2; size >= 0; size--) {
    for (int k = 1; k >=0; k--) {
      int j = b % 4;
      b = b / 4;
      int i = b % 4;
      b = b / 4;
      white_locations[size].push_back(std::make_pair(i,j));
    }
  }

  for (int size = 0; size < 3; size++) {
    for (auto [i, j] : white_locations[size]) {
      if (i == 3) {
	out.white_pieces[size] += 1;
      } else {
	out.positions[i][j] = add_to_position(out.positions[i][j], size, W);
      }
    }
  }

  for (int size = 0; size < 3; size++) {
    for (auto [i, j] : black_locations[size]) {
      if (i == 3) {
	out.black_pieces[size] += 1;
      } else {
	out.positions[i][j] = add_to_position(out.positions[i][j], size, B);
      }
    }
  }

  return out;
}

struct Move {
  int8_t color = 0;
  int8_t size = -1; // 0 - biggest
  // Current location of the piece. If -1 this is a new piece
  int8_t from_i;
  int8_t from_j;
  // New location of the piece.
  int8_t to_i;
  int8_t to_j;
};

// returns the biggest size of a piece in the given position. 0 is the biggest. -1 if no piece is placed.
int biggest_size(int position) {
  if (!position)
    return -1;
  for (int k = 0; k < 3; k++) {
    int shift = 2*k;
    if ((0x3 << shift) & position)
      return k;
  }
  return -1;
}

void print_move(const Move& m) {
  char dbg[100];
  sprintf(dbg, "from i: %d, from j: %d, to i: %d, to j: %d, color: %d, size: %d\n", m.from_i, m.from_j, m.to_i, m.to_j, m.color, m.size);
  std::cout << "move: " << dbg;
}

void print_board(const Board& b) {
  std::string s = "\n\n";
  for (int row = 0; row < 17; row++) {
    for (int col = 0; col < 17; col++) {
      if (row == 5 || row == 11) {
	s += "-";
	continue;
      }
      if (col == 5 || col == 11) {
	s+= "|";
	continue;
      }
      int i = row / 6;
      int j = col / 6;
      int position = b.positions[i][j];
      i = row - i*6;
      j = col - j*6;
      int k = 1;
      if (i == 2 && j == 2) {
	k = 2;
      } else if (i == 0 || i == 4 || j == 0 || j == 4) {
	k = 0;
      }
      int color = sub_pos(position, k);
      if (color == W) {
	s += "\033[1;43m \033[0m";
      } else if (color == B) {
	s += "\033[1;44m \033[0m";
      } else {
	s += " ";
      }
    }
    s += "\n";
  }
  std::cout << s;

  s = "\nAvailable Pieces:\n";
  s = "Color | Big | Medium | Small\n";
  s += "\033[1;43m \033[0m";
  for (int i = 0; i < 3; i++) {
    char s2[10];
    sprintf(s2, "       %d", b.white_pieces[i]);
    s += s2;
  }
  s += "\n\033[1;44m \033[0m";
  for (int i = 0; i < 3; i++) {
    char s2[10];
    sprintf(s2, "       %d", b.black_pieces[i]);
    s += s2;
  }
  char s2[30];

  #ifdef LIMIT_TOTAL_MOVES
  sprintf(s2, "\nMoves so far: %d", b.moves_so_far);
  s += s2;
  #endif
  s += "\nTo move:";
  s += b.move == W ?  "\033[1;43m \033[0m" : "\033[1;44m \033[0m";
  s += "\n";

  std::cout << s;
}

// Effective color in the given position
int effective(int position) {
  int k = biggest_size(position);
  if (k < 0) return 0;
  return sub_pos(position,k);
}

bool is_board_consistent(const Board& b) {
  if (b.move != W && b.move != B) {
    return false;
  }

  #ifdef LIMIT_TOTAL_MOVES
  if (b.moves_so_far > MOVES_TO_DRAW) {
    return false;
  }
  #endif
  // count the number of pieces of each color and size, [color][size]
  int piece_count_by_color[3][3] = {0};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
	int color = sub_pos(b.positions[i][j], k);
	piece_count_by_color[color][k] += 1;
      }
    }
  }
  for (int k = 0; k < 3; k++) {
    piece_count_by_color[W][k] += b.white_pieces[k];
    piece_count_by_color[B][k] += b.black_pieces[k];
  }

  for (int k = 0; k < 3; k++) {
    if (piece_count_by_color[W][k] != 2)
      return false;
    if (piece_count_by_color[B][k] != 2)
      return false;
  }
  return true;
}

// Applied the given move to the given board to get a new board position.
// Returns false if the move cannot be applied.
bool apply_move(const Board& b, const Move& m, Board& new_b) {
  new_b = b;
  #ifdef DEBUG
  std::cout << "DBG: apply_move()\n";
  print_move(m);
  if (!is_board_consistent(b)) {
    std::cout << "DBG: inconsisten board: ";
    print_board(b);
    abort();
  }
  if (b.move != m.color) {
    std::cout << "DBG: inconsisten color: ";
    char dbg[50];
    sprintf(dbg, "move color unexpected: b.move = %d, m.color = %d\n", b.move, m.color);
    std::cout << "DBG: " << dbg;
    abort();
  }
  #endif
  if (m.from_i == -1) {
    // New piece. Reduce the corresponding counter.
    if (m.color == W) {
      #ifdef DEBUG
      if (b.white_pieces[m.size] == 0) {
	std::cout << "DBG: no white piece of given size: ";
	print_move(m);
	print_board(b);
	std::cout << "DBG: no white piece";
	abort();
      }
      #endif
      new_b.white_pieces[m.size] -= 1;
    } else if (m.color == B) {
      if (b.black_pieces[m.size] == 0) return false;
      new_b.black_pieces[m.size] -= 1;
    } else {
      return false;
    }
  } else {
    // Has to move it.
    if (m.from_i == m.to_i && m.from_j == m.to_j) return false;

     // Existing piece. Check that it's a top piece in the position, and that it's the right color.
    int from_position = b.positions[m.from_i][m.from_j];
    if (biggest_size(from_position) != m.size) return false;
    if (sub_pos(from_position, m.size) != m.color) return false;

    // Remove piece
    new_b.positions[m.from_i][m.from_j] = remove_from_position(from_position,  m.size);
  }
  // Check new position
  int to_position = b.positions[m.to_i][m.to_j];
  int k = biggest_size(to_position);
  if (k != -1 and k <= m.size) return false;

  // Add piece
  new_b.positions[m.to_i][m.to_j] = add_to_position(to_position, m.size, m.color);
  new_b.move = b.move == W ? B : W;

  #ifdef LIMIT_TOTAL_MOVES
  new_b.moves_so_far += 1;
  #endif
  return is_board_consistent(new_b);
}

void test_printing() {
  Board b = {0};
  b.positions[0][0] = add_to_position(b.positions[0][0], 0, W);
  b.positions[0][0] = add_to_position(b.positions[0][0], 1, B);
  b.positions[0][0] = add_to_position(b.positions[0][0], 2, W);
  b.positions[0][1] = add_to_position(b.positions[0][1], 0, B);
  b.positions[0][1] = add_to_position(b.positions[0][1], 1, W);
  b.positions[0][1] = add_to_position(b.positions[0][1], 2, B);
  b.positions[1][1] = add_to_position(b.positions[0][0], 0, W);
  b.positions[1][1] = add_to_position(b.positions[0][0], 1, B);
  b.positions[1][1] = add_to_position(b.positions[0][0], 2, W);
  b.positions[2][1] = add_to_position(b.positions[0][1], 0, B);
  b.positions[2][1] = add_to_position(b.positions[0][1], 1, W);
  b.positions[2][1] = add_to_position(b.positions[0][1], 2, B);
  print_board(b);
}

Board init_board() {
  Board b = {0};
  b.move = W;
  for (int i = 0; i < 3; i++) {
    b.white_pieces[i] = 2;
    b.black_pieces[i] = 2;
  }
  return b;
}

void test_moves() {
  Board b = init_board();
  Move m = {.from_i = -1, .from_j = -1, .to_i = 1, .to_j = 1, .size = 1, .color = W};
  Board b2;
  std::cout << apply_move(b, m, b2) << "\n";
  print_board(b2);

  b = b2;
  m = {.from_i = -1, .from_j = -1, .to_i = 1, .to_j = 1, .size = 0, .color = B};
  std::cout << apply_move(b, m, b2) << "\n";
  print_board(b2);

  b = b2;
  m = {.from_i = -1, .from_j = -1, .to_i = 2, .to_j = 2, .size = 2, .color = W};
  std::cout << apply_move(b, m, b2) << "\n";
  print_board(b2);

  b = b2;
  m = {.from_i = -1, .from_j = -1, .to_i = 2, .to_j = 2, .size = 0, .color = B};
  std::cout << apply_move(b, m, b2) << "\n";
  print_board(b2);
  print_board(Decompress(Compress(b2)));
}

void effective_positions(const int positions[3][3], int(&e)[3][3]) {
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      e[i][j] = effective(positions[i][j]);
}

// Whether the given color is winning the given effective board
bool winner(const int e[3][3], int color) {
  for (int i = 0; i < 3; i++) {
    // row i
    bool win = true;
    for (int j = 0; j < 3; j++)
      win &= e[i][j] == color;
    if (win)
      return true;
    // column i
    win = true;
    for (int j = 0; j < 3; j++)
      win &= e[j][i] == color;
    if (win)
      return true;
  }

  // Main diagonal
  bool win = true;
  for (int i = 0; i < 3; i++)
    win &= e[i][i] == color;
  if (win)
    return true;
  // Other diagonal
  win = true;
  for (int i = 0; i < 3; i++)
    win &= e[i][2-i] == color;
  return win;
}

void test_winning() {
  Board b = init_board();
  Board b2;
  Move m = {0};
  m = {.from_i = -1, .from_j = -1, .to_i = 0, .to_j = 0, .size = 2, .color = W};
  std::cout << apply_move(b, m, b2) << "\n";
  m = {.from_i = -1, .from_j = -1, .to_i = 1, .to_j = 1, .size = 0, .color = B};
  std::cout << apply_move(b2, m, b) << "\n";
  m = {.from_i = -1, .from_j = -1, .to_i = 0, .to_j = 0, .size = 1, .color = W};
  std::cout << apply_move(b, m, b2) << "\n";
  m = {.from_i = -1, .from_j = -1, .to_i = 0, .to_j = 1, .size = 1, .color = B};
  std::cout << apply_move(b2, m, b) << "\n";
  m = {.from_i = -1, .from_j = -1, .to_i = 0, .to_j = 0, .size = 0, .color = W};
  std::cout << apply_move(b, m, b2) << "\n";
  m = {.from_i = -1, .from_j = -1, .to_i = 2, .to_j = 1, .size = 1, .color = B};
  std::cout << apply_move(b2, m, b) << "\n";
  print_board(b);

  int e[3][3];
  effective_positions(b.positions, e);
  for (int i = 0; i <3; i++)
    for (int j = 0; j < 3; j++)
      b.positions[i][j] = e[i][j];
  print_board(b);
  std::cout << "White winner? " << winner(e, W) << "\n";
  std::cout << "Black winner? " << winner(e, B) << "\n";
}

std::vector<Move> next_moves_with_piece(const int positions[3][3], int8_t color, int8_t size, int8_t from_i, int8_t from_j) {
  #ifdef DEBUG
  std::cout << "next_moves_with_piece(color = " << static_cast<int>(color) << ", size = " << static_cast<int>(size) << ", from: " << static_cast<int>(from_i) << ", " << static_cast<int>(from_j) << ")";
  #endif
  std::vector<Move> out;
  for (int8_t i = 0; i < 3; i++) {
    for (int8_t j = 0; j < 3; j++) {
      if (i == from_i && j == from_j) continue;
      int8_t biggest = biggest_size(positions[i][j]);
      if (biggest == -1 || biggest > size) {
	Move m = {.size = size, .color = color, .from_i = from_i, .from_j = from_j, .to_i = i, .to_j = j};
	out.push_back(m);
      }
    }
  }
  #ifdef DEBUG
  std::cout << " # moves = " << out.size() << "\n";
  #endif
  return out;
}

std::vector<Move> next_moves_with_new_pieces(const Board& b) {
  std::vector<Move> out;
  const int* available_pieces = b.move == W ? b.white_pieces : b.black_pieces;
  for (int size = 0; size < 3; size++) {
    if (available_pieces[size] > 0) {
      std::vector<Move> v = next_moves_with_piece(b.positions, b.move, size, -1, -1);
      out.insert(out.end(), v.begin(), v.end());
    }
  }
  return out;
}

void set_positions(Board& b, int p[3][3]) {
  for (int i = 0; i < 3; i++)
    for (int j=0; j < 3; j++)
      b.positions[i][j] = p[i][j];
}

std::vector<Move> next_moves_with_existing_pieces(const Board& b) {
  std::vector<Move> out;
  #ifdef DEBUG
  std::cout << "Next move with existing pieces()\n";
  #endif
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      int k = biggest_size(b.positions[i][j]);
      if (sub_pos(b.positions[i][j],k) == b.move) {
	Board dummy = b;
	dummy.positions[i][j] = remove_from_position(b.positions[i][j], k);
	int e[3][3];
	effective_positions(dummy.positions, e);
	// Check if the opponent is winning during the move
	#ifdef DEBUG
	//std::cout << "Checking for opponent win en passant, dummy board:\n";
	//print_board(dummy);
	//Board effective = dummy;
	//set_positions(effective, e);
	//std::cout << "Effective board:\n";
	//print_board(effective);
	#endif
	if (winner(e, b.move == W ? B : W)) continue;
	#ifdef DEBUG
	//std::cout << "No win en passant, continue to next_moves_with_piece\n";
	#endif
	std::vector<Move> v = next_moves_with_piece(dummy.positions, b.move, k, i, j);
	out.insert(out.end(), v.begin(), v.end());
      }
    }
  }
  return out;
}

std::vector<Move> next_moves(const Board& b) {
  #ifdef DEBUG
  std::cout << "next_moves():\n";
  print_board(b);
  #endif
  std::vector<Move> out = next_moves_with_new_pieces(b);
  std::vector<Move> v = next_moves_with_existing_pieces(b);
  out.insert(out.end(), v.begin(), v.end());
  #ifdef DEBUG
  std::cout << "next_moves() returns " << out.size() << " moves\n";
  #endif
  return out;
}

void test_next_moves() {
  Board b = init_board();
  print_board(b);
  for (const Move& m: next_moves(b)) {
    Board b2;
    apply_move(b, m, b2);
    for (const Move& m2: next_moves(b2)) {
      Board b3;
      apply_move(b2, m2, b3);
      for (const Move& m3: next_moves(b3)) {
	Board b4;
	apply_move(b3, m3, b4);
	print_board(Decompress(Compress((b4))));
      }
    }
  }
}

struct Metadata {
  Move best_move = {0}; // 6 Bytes
  // W / B / D / 0 (unknown)
  int8_t outcome = 0;
  int32_t moves_to_outcome = -1;
};

static std::unordered_map<int64_t, Metadata> tree = {};
static std::unordered_set<int64_t> visited = {};



void play(const Board& in) {
  Board b = in;
  std::cout << "\n\n\n\n\n\n\n\n\n\n\n LET THE GAME BEGIN!! \n\n\n\n\n\n\n";
  int8_t other = 3 - b.move;
  while (1) {
    print_board(b);
    int64_t c = Compress(b);
    if (!tree.contains(c)) {
      std::cout << "Missing expected state in tree: " << c << "\n";
      print_board(b);
      std::cout << "Recompressed: " << Compress(b) << "\n";
      abort();
    }
    Metadata md = tree[c];
    Board b2;

    std::string w = "noone";
    if (md.outcome == W) {
      w = "\033[1;43m \033[0m";
    } else if (md.outcome == B) {
      w = "\033[1;44m \033[0m";
    }

    std::cout << w << " is winning in " << static_cast<int>(md.moves_to_outcome) << " moves\n";
    if (md.moves_to_outcome > 0) {
      apply_move(b, md.best_move, b2);
      b = b2;
    } else {
      std::cout << "\n\n   THE END \n\n";
      break;
    }
  }
}

void play_known_endings() {
  for (auto const& [compressed, metadata] : tree) {
    play(Decompress(compressed));
  }
}

// Returns the winner of the board as is, -1 if no winner
int8_t winner(Board& b) {
  int e[3][3];
  effective_positions(b.positions, e);
  if (winner(e, W)) {
    return W;
  }
  if (winner(e, B)) {
    return B;
  }
  return -1;
}

void unravel_stack(std::stack<int64_t>& s) {
  std::cout << "\n\n\n\n\n 		UNRAVELING STACK  \n\n\n\n";
  while(!s.empty()) {
    int64_t b_key = s.top();
    Board b = Decompress(b_key);
    s.pop();
    print_board(b);
  }
}

void analyze(const Board& in) {
  std::stack<int64_t> s;
  s.push(Compress(in));
  visited.insert(Compress(in));
  while (!s.empty()) {
    if (tree.size() % (1<<PRINT_TREE_SIZE_RESOLUTION) == 0) {
      std::cout << "tree.size()    = " << tree.size() << "\n";
      std::cout << "stack.size()   = " << s.size() << "\n";
      std::cout << "visited.size() = " << visited.size() << "\n";
      #ifdef TREE_SIZE_LIMIT
      if (tree.size() >= TREE_SIZE_LIMIT) {
        play_known_endings();
	abort();
      }
      if (s.size() >= STACK_SIZE_LIMIT) {
	unravel_stack(s);
	abort();
      }
      #endif
    }

    int64_t b_key = s.top();
    Board b = Decompress(b_key);
    int8_t other = 3 - b.move;

    #ifdef DEBUG
    if (tree.contains(b_key)) {
      // TODO: This is probably wrong. It could be that we encountered this possition downstream
      // and marked it as a D. Better check that it's a D with 0 moves to outcome
      // (although that too might not be the only case...).
      std::cout << "Did not expect this key in the tree.";
      abort();
    }
    #endif

    int8_t w = winner(b);
    if (w > -1) {
      tree[b_key] = {.outcome = w, .moves_to_outcome = 0};
      s.pop();
      visited.erase(b_key);
      #ifdef DEBUG
      std::cout << "Found terminal board:\n";
      print_board(b);
      #endif
      continue;
    }

    // First pass - look for win in 1.
    auto next = next_moves(b);
    bool found_winner = false;
    for (const Move& m: next) {
      Board new_b;
      apply_move(b, m, new_b);
      int64_t new_b_key = Compress(new_b);
      if (winner(new_b) == b.move) {
	// Win in 1
	tree[new_b_key] = {.outcome = b.move, .moves_to_outcome = 0};
	tree[b_key] = {.best_move = m, .outcome = b.move, .moves_to_outcome = 1};
        s.pop();
        visited.erase(b_key);
	found_winner = true;
	break;
      }
    }
    if (found_winner)
      continue;

    // Second pass - look for any winner, or push a board on the stack to go deeper
    int64_t board_to_push = -1;
    for (const Move& m: next) {
      Board new_b;
      apply_move(b, m, new_b);
      int64_t new_b_key = Compress(new_b);
      if (!tree.contains(new_b_key)) {
	if (board_to_push == -1 && !visited.contains(new_b_key)) {
	  board_to_push = new_b_key;
	}
      } else {
	Metadata n_md = tree[new_b_key];
	if (n_md.outcome == b.move) {
	  // Found a winning move, no point to continue
	  tree[b_key] = {.best_move = m, .outcome = b.move, .moves_to_outcome = n_md.moves_to_outcome + 1};
          s.pop();
          visited.erase(b_key);
	  found_winner = true;
	  break;
	}
      }
    }
    if (found_winner)
      continue;
    if (board_to_push > -1) {
      // We'll get back to current board b after children are analyzed.
      s.push(board_to_push);
      visited.insert(board_to_push);
      continue;
    }

    // Nothing pushed, we can compute the next best move.
    #ifdef DEBUG
    std::cout << "Looking for best move amongs " << next.size() << " possible moves\n";
    #endif
    Move best_move;
    int32_t moves_to_best = -1;
    int8_t best_outcome = other;
    for (const Move& m: next) {
      Board new_b;
      apply_move(b, m, new_b);
      int64_t new_b_key = Compress(new_b);
      if (visited.contains(new_b_key)) {
	#ifdef DEBUG
        std::cout << "Found a draw by repetition\n";
        #endif
        moves_to_best = 1;
        best_move = m;
	best_outcome = D;
	// That's the best we can get at this point.
	break;
      }

      if (!tree.contains(new_b_key)) {
	std::cout << "Key missing in tree when expected - aborting\n";
	abort();
      }
      Metadata n_md = tree[new_b_key];
      if (n_md.outcome == D && (moves_to_best == -1 || moves_to_best > n_md.moves_to_outcome)) {
	#ifdef DEBUG
        std::cout << "Found a draw\n";
        #endif
	moves_to_best = n_md.moves_to_outcome + 1;
	best_move = m;
	best_outcome = D;
      }
      if (best_outcome == D) {
	// We can't improve.
	continue;
      }

      #ifdef DEBUG
      std::cout << "Found a losing move\n";
      #endif
      if (n_md.moves_to_outcome >= moves_to_best) {
        moves_to_best = n_md.moves_to_outcome + 1;
	best_move = m;
      }
    }
    tree[b_key] = {.best_move = best_move, .outcome = best_outcome, .moves_to_outcome = moves_to_best};
    s.pop();
    visited.erase(b_key);
  }
}

void move(Board& b, int8_t color, int8_t size, int8_t i, int8_t j, int8_t from_i = -1, int8_t from_j = -1) {
  Move m = {.from_i = from_i, .from_j = from_j, .to_i = i, .to_j = j, .size = size, .color = color};
  Board out;
  bool status = apply_move(b, m, out);
  #ifdef DEBUG
  if (!status) {
    std::cout << "Failed to move";
  }
  #endif
  b = out;
}

// Writes board outcomes into a file
void dump_to_file() {
  std::ofstream file("outcomes.csv");
  for (const auto& [k, v] : tree) {
    file << k << ", ";
    file << static_cast<int>(v.best_move.color) << ",";
    file << static_cast<int>(v.best_move.size) << ",";
    file << static_cast<int>(v.best_move.from_i) << ",";
    file << static_cast<int>(v.best_move.from_j) << ",";
    file << static_cast<int>(v.best_move.to_i) << ",";
    file << static_cast<int>(v.best_move.to_j) << ",";
    file << static_cast<int>(v.outcome) << ",";
    file << v.moves_to_outcome << "\n";
  }
  file.close();
}

void read_from_file() {
  std::ifstream file("outcomes.csv");

  long count = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (count % (1 << PRINT_TREE_SIZE_RESOLUTION) == 0) {
      std::cout << "Read " << count << " entries from file\n";
    }
    count += 1;
    std::stringstream ss(line);
    std::string item;

    std::vector<std::string> values;
    while (std::getline(ss, item, ',')) {
      values.push_back(item);
    }
    if (values.size() != 9) {
      std::cout << "Unexpected number of values in line: " << line << "\n";
      abort();
    }

    int64_t k = std::stol(values[0]);
    Metadata v;
    v.best_move.color = std::stoi(values[1]);
    v.best_move.size = std::stoi(values[2]);
    v.best_move.from_i = std::stoi(values[3]);
    v.best_move.from_j = std::stoi(values[4]);
    v.best_move.to_i = std::stoi(values[5]);
    v.best_move.to_j = std::stoi(values[6]);
    v.outcome = std::stoi(values[7]);
    v.moves_to_outcome = std::stoi(values[8]);

    tree[k] = v;
  }
  file.close();
}

void min_max() {
  Board b = init_board();
  analyze(b);

  // Also analyze for every W first move to learn optimal play as B.
  int i = 1;
  for (const Move& m: next_moves(b)) {
    std::cout << "After analyzing " << i << " initial positions hash size is: " << tree.size();
    Board new_b;
    apply_move(b, m, new_b);
    std::cout << "\nAnalyzing starting from:\n";
    print_board(new_b);
    analyze(new_b);
    i += 1;
  }

  if (DUMP_TO_FILE) {
    dump_to_file();
  }
}


void from_position() {
  Board b = init_board();
  move(b, W, 0, 0, 0);
  move(b, B, 0 ,0, 1);
  move(b, W, 2, 1, 2);
  move(b, B, 1 ,1, 2);
  move(b, W, 2, 2, 1);
  move(b, B, 2 ,1, 0);
  move(b, W, 1, 1, 0);
  //move(b, B, 1 ,1, 1);
  //move(b, W, 0, 1, 1);
  //move(b, B, 2 ,2, 2);
  //move(b, W, 1, 2, 2);
  //move(b, B, 0 ,2, 2);
  std::cout << "Starting from board:\n";
  print_board(b);
  analyze(b);
  play(b);
}

void play();

Move get_user_move(const Board& board) {
  int8_t size;
  int8_t from_i = -1;
  int8_t from_j = -1;
  int8_t to_i;
  int8_t to_j;

  int choice;
  std::cout << "Would you like to...\n\n";
  std::cout << "1. Play a new piece\n";
  std::cout << "2. Move a piece already on the board\n";
  std::cout << "9. Exit to main menu\n";

  std::cin >> choice;
  if (choice == 9) {
    return {.size = -1};
  } else if (choice == 1) {
    std::cout << "Choose a size for your piece...\n";
    std::cout << "1. Big\n";
    std::cout << "2. Medium\n";
    std::cout << "3. Small\n";
    std::cin >> choice;
    size = static_cast<int8_t>(choice-1);
  } else if (choice == 2) {
    std::cout << "Which piece would you like to move?\n";
    std::cout << "   1  |  2  |  3\n";
    std::cout << "-----------------\n";
    std::cout << "   4  |  5  |  6\n";
    std::cout << "-----------------\n";
    std::cout << "   7  |  8  |  9\n";
    std::cin >> choice;
    choice -= 1;
    from_i = choice / 3;
    from_j = choice % 3;
    size = biggest_size(board.positions[from_i][from_j]);
  } else {
    return get_user_move(board);
  }
  std::cout << "Where would you like to move it?\n";
  std::cout << "   1  |  2  |  3\n";
  std::cout << "-----------------\n";
  std::cout << "   4  |  5  |  6\n";
  std::cout << "-----------------\n";
  std::cout << "   7  |  8  |  9\n";
  std::cin >> choice;
  choice -= 1;
  to_i = choice / 3;
  to_j = choice % 3;
  return {.size = size, .color = board.move, .from_i = from_i, .from_j = from_j, .to_i = to_i, .to_j = to_j};
}

// Applied the given move to the given board to get a new board position.
// Returns false if the move cannot be applied.
// Similar to apply_move() but with ehnacned safety checks.
bool safe_apply_move(const Board& b, const Move& m, Board& new_b) {
  new_b = b;
  if (!is_board_consistent(b)) {
    std::cout << "DBG: inconsisten board: ";
    print_board(b);
    abort();
  }
  if (b.move != m.color) {
    std::cout << "DBG: inconsisten color: ";
    char dbg[50];
    sprintf(dbg, "move color unexpected: b.move = %d, m.color = %d\n", b.move, m.color);
    std::cout << "DBG: " << dbg;
    abort();
  }
  if (m.from_i == -1) {
    // New piece. Reduce the corresponding counter.
    if (m.color == W) {
      if (b.white_pieces[m.size] == 0)
	return false;
      new_b.white_pieces[m.size] -= 1;
    } else if (m.color == B) {
      if (b.black_pieces[m.size] == 0)
	return false;
      new_b.black_pieces[m.size] -= 1;
    } else {
      return false;
    }
  } else {
    // Has to move it.
    if (m.from_i == m.to_i && m.from_j == m.to_j) return false;

     // Existing piece. Check that it's a top piece in the position, and that it's the right color.
    int from_position = b.positions[m.from_i][m.from_j];
    if (biggest_size(from_position) != m.size) return false;
    if (sub_pos(from_position, m.size) != m.color) return false;

    // Remove piece
    new_b.positions[m.from_i][m.from_j] = remove_from_position(from_position,  m.size);
  }
  // Check new position
  int to_position = b.positions[m.to_i][m.to_j];
  int k = biggest_size(to_position);
  if (k != -1 and k <= m.size) return false;

  // Add piece
  new_b.positions[m.to_i][m.to_j] = add_to_position(to_position, m.size, m.color);
  new_b.move = b.move == W ? B : W;

  return is_board_consistent(new_b);
}

void play() {
  int choice;
  int roboplayer = 99;
  while (roboplayer == 99) {
    std::cout << "Welcome! Choose an option:\n";
    std::cout << "1. Play Orange (first player to move)\n";
    std::cout << "2. Play Blue\n";
    std::cout << "3. Play For Both Sides (a.k.a Analyze Position)\n";
    std::cout << "4. Exit\n";

    std::cin >> choice;
    switch (choice) {
      case 1:
	roboplayer = B;
	break;
      case 2:
	roboplayer = W;
	break;
      case 3:
	roboplayer = -1;
	break;
      case 4:
	std::cout << "Farewell...\n\n";
	return;
      default:
	break;
    }
  }

  std::cout << "\n\n\n\n\n\n\n\n\n\n\n LET THE GAME BEGIN!! \n\n\n\n\n\n\n";
  Board b = init_board();
  int move_count = 0;
  std::unordered_map<int64_t, int> encountered_positions;
  while (1) {
    std::cout << "\n\nBoard state after " << move_count << " moves:\n";
    print_board(b);
    int64_t c = Compress(b);
    if (encountered_positions.contains(c)) {
      std::cout << "\n\n DRAW BY REPETITION ... THANKS FOR PLAYING\n\n";
      std::cout << "Position already occured in move: " << encountered_positions[c];
      return play();
    }

    Metadata md = {0};
    if (!tree.contains(c)) {
      std::cout << "Missing expected state in tree: " << c << "\n";
      std::cout << "Contact the development team\n";
      md.moves_to_outcome = -1;
      if (b.move == roboplayer) {
        std::cout << "Resetting... \n";
	return play();
      }
    }
    md = tree[c];

    std::string w = "noone";
    if (md.outcome == W) {
      w = "\033[1;43m \033[0m";
    } else if (md.outcome == B) {
      w = "\033[1;44m \033[0m";
    }
    std::cout << "\n[Analysis]: " << w << " is winning in " << static_cast<int>(md.moves_to_outcome) << " moves\n";

    if (md.moves_to_outcome == 0) {
      std::cout << "\n\n   THE END ... THANKS FOR PLAYING \n\n";
      return play();
    }

    std::string color;
    if (b.move == W) {
      color  = "\033[1;43m \033[0m";
    } else if (b.move == B) {
      color = "\033[1;44m \033[0m";
    }

    std::cout << "\n" << color << "'s move\n";

    Move move = b.move == roboplayer ? md.best_move : get_user_move(b);
    Board b2;
    if (move.size == -1) {
      // Special exit code.
      return play();
    }
    if (!safe_apply_move(b, move, b2)) {
      std::cout << "Illegal move, try again...\n\n";
      continue;
    }
    encountered_positions[c] = move_count;
    b = b2;
    move_count += 1;
  }
}

int main() {
  if (FROM_FILE) {
    read_from_file();
  } else {
    min_max();
  }
  play();
  //from_position();
  //play(init_board());
  return 0;
}
