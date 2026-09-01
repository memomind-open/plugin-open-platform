#ifndef BREAKOUT_TRANSLATIONS_H
#define BREAKOUT_TRANSLATIONS_H

typedef struct {
    const char *tag;
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
        .score = "得分：",
        .game_over = "游戏结束",
        .victory = "你赢了！",
        .time = "时间：",
        .restart = "单击重新开始",
        .pause = "暂停中\n单击继续",
        .control_hint = "抬头/长按退出",
        .exit_prefix = "",
        .exit_suffix = "s后退出",
    },
    {
        .tag = "en",
        .score = "Score: ",
        .game_over = "Game Over!",
        .victory = "You Win!",
        .time = "Time: ",
        .restart = "Click to restart",
        .pause = "Paused\nClick to continue",
        .control_hint = "Look up / hold: exit",
        .exit_prefix = "Exit ",
        .exit_suffix = "s",
    },
    {
        .tag = "ja",
        .score = "スコア：",
        .game_over = "ゲームオーバー！",
        .victory = "あなたの勝ちです！",
        .time = "時間：",
        .restart = "クリックして再スタート",
        .pause = "一時停止中\nクリックして続行",
        .control_hint = "上向き/長押し終了",
        .exit_prefix = "",
        .exit_suffix = "秒後に終了",
    },
    {
        .tag = "ko",
        .score = "점수: ",
        .game_over = "게임 오버!",
        .victory = "당신이 이겼습니다!",
        .time = "시간: ",
        .restart = "클릭하여 재시작",
        .pause = "일시정지\n클릭하여 계속",
        .control_hint = "위 보기/길게: 종료",
        .exit_prefix = "",
        .exit_suffix = "초 후 종료",
    },
    {
        .tag = "de",
        .score = "Punkte: ",
        .game_over = "Game Over!",
        .victory = "Du hast gewonnen!",
        .time = "Zeit: ",
        .restart = "Klicken zum Neustart",
        .pause = "Pausiert\nKlicken zum Fortsetzen",
        .control_hint = "Hoch / halten: Ende",
        .exit_prefix = "Ende ",
        .exit_suffix = "s",
    },
    {
        .tag = "fr",
        .score = "Score : ",
        .game_over = "Game Over !",
        .victory = "Vous avez gagné !",
        .time = "Temps : ",
        .restart = "Cliquez pour rejouer",
        .pause = "En pause\nCliquez pour continuer",
        .control_hint = "Lever / tenir: quitter",
        .exit_prefix = "Quitter ",
        .exit_suffix = "s",
    },
    {
        .tag = "es",
        .score = "Puntuación: ",
        .game_over = "¡Game Over!",
        .victory = "¡Has ganado!",
        .time = "Tiempo: ",
        .restart = "Haga clic para reiniciar",
        .pause = "Pausado\nHaga clic para continuar",
        .control_hint = "Arriba / mantener: salir",
        .exit_prefix = "Salir ",
        .exit_suffix = "s",
    },
    {
        .tag = "it",
        .score = "Punteggio: ",
        .game_over = "Game Over!",
        .victory = "Hai vinto!",
        .time = "Tempo: ",
        .restart = "Clicca per riavviare",
        .pause = "In pausa\nClicca per continuare",
        .control_hint = "Su / tieni: esci",
        .exit_prefix = "Uscita ",
        .exit_suffix = "s",
    },
    {
        .tag = "ru",
        .score = "Счет: ",
        .game_over = "Игра окончена!",
        .victory = "Вы выиграли!",
        .time = "Время: ",
        .restart = "Нажмите, чтобы перезапустить",
        .pause = "Пауза\nНажмите, чтобы продолжить",
        .control_hint = "Вверх / удерж.: выход",
        .exit_prefix = "Выход ",
        .exit_suffix = "с",
    },
    {
        .tag = "pt",
        .score = "Pontuação: ",
        .game_over = "Game Over!",
        .victory = "Você venceu!",
        .time = "Tempo: ",
        .restart = "Clique para reiniciar",
        .pause = "Pausado\nClique para continuar",
        .control_hint = "Acima / segure: sair",
        .exit_prefix = "Sair ",
        .exit_suffix = "s",
    },
    {
        .tag = "zh-TW",
        .score = "分數：",
        .game_over = "遊戲結束",
        .victory = "你贏了！",
        .time = "時間：",
        .restart = "單擊重新開始",
        .pause = "暫停中\n單擊繼續",
        .control_hint = "抬頭/長按退出",
        .exit_prefix = "",
        .exit_suffix = "s後退出",
    },
};

#endif
