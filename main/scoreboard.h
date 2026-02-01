/*
 * Scoreboard - Game state and logic management
 * for Digital Table Tennis Scoreboard
 */

#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include <stdbool.h>
#include <stdint.h>

// Game state enum
typedef enum {
    GAME_STATE_CONNECTING,     // BT 연결 중
    GAME_STATE_CONNECTED,      // 연결됨 (초기화 중)
    GAME_STATE_DISCONNECTED,   // 연결 끊김
    GAME_STATE_RULE_SELECT,    // 룰 선택 화면
    GAME_STATE_SELECT_FIRST,   // 선공 선택 대기
    GAME_STATE_PLAYING,        // 게임 진행 중
    GAME_STATE_CONFIRM_RESET,  // 리셋 확인 대기
    GAME_STATE_MENU,           // 강제 옵션 메뉴
    GAME_STATE_WINNER,         // 세트 승자 결정
    GAME_STATE_COURT_CHANGE,   // 코트 체인지 알림 (단식 마지막세트)
    GAME_STATE_MATCH_END,      // 경기 종료
} game_state_t;

// Undo history entry
typedef struct {
    int left_score;
    int right_score;
    int serve_side;
} score_history_t;

// Scoreboard state
typedef struct {
    game_state_t state;
    int left_score;
    int right_score;
    int left_sets;
    int right_sets;
    int serve_side;         // 0=left, 1=right
    int first_serve_side;   // 첫 세트 시작 서브 측 (다음 세트 시작시 사용)
    int undo_count;         // 남은 취소 횟수 (max 2)
    score_history_t history[2];  // undo용 히스토리
    int history_index;      // 현재 히스토리 인덱스
    int menu_selection;     // 메뉴 선택 (0=Switch, 1=Stop)
    int winner_side;        // 세트 승자 (0=left, 1=right, -1=none)
    // Game configuration (set in rule select screen)
    bool is_doubles;        // true=복식(2:2), false=단식(1:1)
    int sets_to_win;        // 승리에 필요한 세트 수 (2 or 3)
    int total_sets;         // 총 세트 수 (3 or 5)
    int rule_cursor;        // 룰 선택 커서 (0=mode, 1=sets)
    bool rule_ok_pressed;   // 첫 번째 OK 눌렸는지 여부
    bool court_changed;     // 현재 세트에서 코트 체인지 완료 여부
    // Set history for display (who won each set: 0=left, 1=right, -1=not played)
    int set_history[5];     // Max 5 sets
    int current_set;        // Current set number (0-based)
} scoreboard_t;

// Game configuration
#define WINNING_SCORE 11
#define DEUCE_SCORE 10
#define SETS_TO_WIN 3
#define TOTAL_SETS 5
#define MAX_UNDO 2

/**
 * Initialize scoreboard to default state
 */
void scoreboard_init(scoreboard_t *sb);

/**
 * Add a point to the specified side
 * @param sb Scoreboard state
 * @param side 0=left, 1=right
 */
void scoreboard_add_point(scoreboard_t *sb, int side);

/**
 * Undo the last point
 * @param sb Scoreboard state
 * @return true if undo was successful, false if no history
 */
bool scoreboard_undo(scoreboard_t *sb);

/**
 * Check if there's a winner for the current set
 * @param sb Scoreboard state
 * @return 0=left won, 1=right won, -1=no winner yet
 */
int scoreboard_check_winner(scoreboard_t *sb);

/**
 * Advance to the next set
 * @param sb Scoreboard state
 */
void scoreboard_next_set(scoreboard_t *sb);

/**
 * Reset the entire match
 * @param sb Scoreboard state
 */
void scoreboard_reset(scoreboard_t *sb);

/**
 * Switch serve side (for forced switch)
 * @param sb Scoreboard state
 */
void scoreboard_switch_serve(scoreboard_t *sb);

/**
 * Force end current set with specified winner
 * @param sb Scoreboard state
 * @param winner 0=left, 1=right
 */
void scoreboard_force_end_set(scoreboard_t *sb, int winner);

/**
 * Check if match is complete (one side has won 3 sets)
 * @param sb Scoreboard state
 * @return 0=left won match, 1=right won match, -1=match not over
 */
int scoreboard_check_match_winner(scoreboard_t *sb);

/**
 * Swap scores between left and right (for court change)
 * @param sb Scoreboard state
 */
void scoreboard_swap_sides(scoreboard_t *sb);

/**
 * Check if current set is the final (deciding) set
 * @param sb Scoreboard state
 * @return true if this is the final set
 */
bool scoreboard_is_final_set(scoreboard_t *sb);

#endif // SCOREBOARD_H
