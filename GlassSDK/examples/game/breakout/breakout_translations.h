#ifndef BREAKOUT_TRANSLATIONS_H
#define BREAKOUT_TRANSLATIONS_H

typedef struct {
    const char *tag;
    const char *hold_exit;
    const char *score;
    const char *game_over;
    const char *victory;
    const char *time;
    const char *restart;
    const char *pause;
    const char *control_hint;
    const char *exit_prefix;
    const char *exit_suffix;
} breakout_strings_t;

static const breakout_strings_t translations[] = {
    {
        .tag = "zh-CN",
        .hold_exit = "按住2s结束",
        .score = "分数：",
        .game_over = "游戏结束",
        .victory = "你赢了！",
        .time = "时间：",
        .restart = "单击重新开始",
        .pause = "暂停中\n单击继续",
        .control_hint = "长按/抬头3秒退出",
        .exit_prefix = "",
        .exit_suffix = "s后退出",
    },
    {
        .tag = "en",
        .hold_exit = "Hold for 2s to exit",
        .score = "Score: ",
        .game_over = "Game Over!",
        .victory = "You Win!",
        .time = "Time: ",
        .restart = "Click to restart",
        .pause = "Paused\nClick to continue",
        .control_hint = "Hold / look up 3s to exit",
        .exit_prefix = "",
        .exit_suffix = "s to Exit",
    },
    {
        .tag = "ja",
        .hold_exit = "2秒間長押しで終了",
        .score = "スコア: ",
        .game_over = "ゲームオーバー！",
        .victory = "あなたの勝ちです！",
        .time = "時間: ",
        .restart = "クリックして再スタート",
        .pause = "一時停止中\nクリックして続行",
        .control_hint = "長押し／上向き3秒で終了",
        .exit_prefix = "",
        .exit_suffix = "s後に終了",
    },
    {
        .tag = "ko",
        .hold_exit = "2초 동안 길게 눌러 종료",
        .score = "점수: ",
        .game_over = "게임 오버!",
        .victory = "당신이 이겼습니다!",
        .time = "시간: ",
        .restart = "클릭하여 재시작",
        .pause = "일시정지\n클릭하여 계속",
        .control_hint = "길게 누르기 / 3초 위 보기: 종료",
        .exit_prefix = "",
        .exit_suffix = "s 후 종료",
    },
    {
        .tag = "de",
        .hold_exit = "2 s gedrückt halten, um zu beenden",
        .score = "Punkte: ",
        .game_over = "Game Over!",
        .victory = "Du hast gewonnen!",
        .time = "Zeit: ",
        .restart = "Klicken zum Neustart",
        .pause = "Pausiert\nKlicken zum Fortsetzen",
        .control_hint = "Halten / 3 Sek. hochsehen: Ende",
        .exit_prefix = "",
        .exit_suffix = "s bis zum Beenden",
    },
    {
        .tag = "fr",
        .hold_exit = "Maintenez 2 s pour quitter",
        .score = "Score : ",
        .game_over = "Game Over!",
        .victory = "Vous avez gagné !",
        .time = "Temps : ",
        .restart = "Cliquez pour rejouer",
        .pause = "En pause\nCliquez pour continuer",
        .control_hint = "Appui long / lever 3 s : quitter",
        .exit_prefix = "",
        .exit_suffix = "s avant de quitter",
    },
    {
        .tag = "es",
        .hold_exit = "Mantén pulsado 2 s para salir",
        .score = "Puntuación: ",
        .game_over = "¡Game Over!",
        .victory = "¡Has ganado!",
        .time = "Tiempo: ",
        .restart = "Haga clic para reiniciar",
        .pause = "Pausado\nHaga clic para continuar",
        .control_hint = "Mantén / mira arriba 3 s: salir",
        .exit_prefix = "",
        .exit_suffix = "s para salir",
    },
    {
        .tag = "it",
        .hold_exit = "Tieni premuto 2 s per uscire",
        .score = "Punteggio: ",
        .game_over = "Game Over!",
        .victory = "Hai vinto!",
        .time = "Tempo: ",
        .restart = "Clicca per riavviare",
        .pause = "In pausa\nClicca per continuare",
        .control_hint = "Tieni / guarda su 3 s: esci",
        .exit_prefix = "",
        .exit_suffix = "s per uscire",
    },
    {
        .tag = "ru",
        .hold_exit = "Удерживайте 2 с, чтобы выйти",
        .score = "Счет: ",
        .game_over = "Игра окончена!",
        .victory = "Вы выиграли!",
        .time = "Время: ",
        .restart = "Нажмите, чтобы перезапустить",
        .pause = "Пауза\nНажмите, чтобы продолжить",
        .control_hint = "Удерж. / вверх 3 с: выход",
        .exit_prefix = "",
        .exit_suffix = "s до выхода",
    },
    {
        .tag = "pt",
        .hold_exit = "Segure por 2 s para sair",
        .score = "Pontuação: ",
        .game_over = "Game Over!",
        .victory = "Você venceu!",
        .time = "Tempo: ",
        .restart = "Clique para reiniciar",
        .pause = "Pausado\nClique para continuar",
        .control_hint = "Segure / olhe acima 3 s: sair",
        .exit_prefix = "",
        .exit_suffix = "s para sair",
    },
    {
        .tag = "zh-TW",
        .hold_exit = "按住2s結束",
        .score = "分數：",
        .game_over = "遊戲結束",
        .victory = "你贏了！",
        .time = "時間：",
        .restart = "單擊重新開始",
        .pause = "暫停中\n單擊繼續",
        .control_hint = "長按／抬頭3秒退出",
        .exit_prefix = "",
        .exit_suffix = "s後退出",
    },
};

#endif
