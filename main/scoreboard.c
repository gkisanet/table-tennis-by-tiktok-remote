/*
 * Scoreboard - Game state and logic implementation
 * for Digital Table Tennis Scoreboard
 */

#include "scoreboard.h"
#include <string.h>

void scoreboard_init(scoreboard_t *sb) {
  memset(sb, 0, sizeof(scoreboard_t));
  sb->state = GAME_STATE_CONNECTING;
  sb->serve_side = 0; // Will be set during SELECT_FIRST
  sb->undo_count = MAX_UNDO;
  sb->winner_side = -1;
  sb->menu_selection = 0;
}

// Save current state to history before making changes
static void save_history(scoreboard_t *sb) {
  if (sb->history_index < 2) {
    sb->history[sb->history_index].left_score = sb->left_score;
    sb->history[sb->history_index].right_score = sb->right_score;
    sb->history[sb->history_index].serve_side = sb->serve_side;
    sb->history_index++;
  } else {
    // Shift history and add new entry
    sb->history[0] = sb->history[1];
    sb->history[1].left_score = sb->left_score;
    sb->history[1].right_score = sb->right_score;
    sb->history[1].serve_side = sb->serve_side;
  }
}

// Update serve side based on current scores
// Uses first_serve_side as the starting base
static void update_serve(scoreboard_t *sb) {
  int total_points = sb->left_score + sb->right_score;
  int serve_offset;

  // In deuce (both >= 10), serve changes every point
  if (sb->left_score >= DEUCE_SCORE && sb->right_score >= DEUCE_SCORE) {
    serve_offset = total_points % 2;
  } else {
    // Normal: serve changes every 2 points
    serve_offset = (total_points / 2) % 2;
  }

  // Apply offset to first serve side
  sb->serve_side = (sb->first_serve_side + serve_offset) % 2;
}

void scoreboard_add_point(scoreboard_t *sb, int side) {
  if (sb->state != GAME_STATE_PLAYING) {
    return;
  }

  // Save history before change
  save_history(sb);

  // Add point
  if (side == 0) {
    sb->left_score++;
  } else {
    sb->right_score++;
  }

  // Update serve side
  update_serve(sb);

  // Reset undo count on new point
  sb->undo_count = MAX_UNDO;

  // Check for winner
  int winner = scoreboard_check_winner(sb);
  if (winner >= 0) {
    sb->winner_side = winner;
    sb->state = GAME_STATE_WINNER;

    // Add to set count
    if (winner == 0) {
      sb->left_sets++;
    } else {
      sb->right_sets++;
    }

    // Check if match is over
    if (scoreboard_check_match_winner(sb) >= 0) {
      sb->state = GAME_STATE_MATCH_END;
    }
  }
}

bool scoreboard_undo(scoreboard_t *sb) {
  if (sb->undo_count <= 0 || sb->history_index <= 0) {
    return false;
  }

  // Restore from history
  sb->history_index--;
  sb->left_score = sb->history[sb->history_index].left_score;
  sb->right_score = sb->history[sb->history_index].right_score;
  sb->serve_side = sb->history[sb->history_index].serve_side;

  sb->undo_count--;

  return true;
}

int scoreboard_check_winner(scoreboard_t *sb) {
  int left = sb->left_score;
  int right = sb->right_score;

  // Check deuce situation (both >= 10)
  if (left >= DEUCE_SCORE && right >= DEUCE_SCORE) {
    // Need 2 point difference
    if (left - right >= 2)
      return 0;
    if (right - left >= 2)
      return 1;
    return -1;
  }

  // Normal win at 11
  if (left >= WINNING_SCORE)
    return 0;
  if (right >= WINNING_SCORE)
    return 1;

  return -1;
}

void scoreboard_next_set(scoreboard_t *sb) {
  // Reset scores for new set
  sb->left_score = 0;
  sb->right_score = 0;
  sb->undo_count = MAX_UNDO;
  sb->history_index = 0;
  sb->winner_side = -1;

  // Go to select first server
  sb->state = GAME_STATE_SELECT_FIRST;
}

void scoreboard_reset(scoreboard_t *sb) {
  game_state_t saved_state = sb->state;
  scoreboard_init(sb);

  // If we were connected, go to select first, not connecting
  if (saved_state != GAME_STATE_CONNECTING &&
      saved_state != GAME_STATE_DISCONNECTED) {
    sb->state = GAME_STATE_SELECT_FIRST;
  }
}

void scoreboard_switch_serve(scoreboard_t *sb) {
  sb->serve_side = (sb->serve_side == 0) ? 1 : 0;
}

void scoreboard_force_end_set(scoreboard_t *sb, int winner) {
  sb->winner_side = winner;

  // Add to set count
  if (winner == 0) {
    sb->left_sets++;
  } else {
    sb->right_sets++;
  }

  // Check if match is over
  if (scoreboard_check_match_winner(sb) >= 0) {
    sb->state = GAME_STATE_MATCH_END;
  } else {
    sb->state = GAME_STATE_WINNER;
  }
}

int scoreboard_check_match_winner(scoreboard_t *sb) {
  if (sb->left_sets >= SETS_TO_WIN)
    return 0;
  if (sb->right_sets >= SETS_TO_WIN)
    return 1;
  return -1;
}
