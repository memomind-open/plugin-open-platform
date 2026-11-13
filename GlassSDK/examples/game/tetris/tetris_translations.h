#ifndef TETRIS_TRANSLATIONS_H
#define TETRIS_TRANSLATIONS_H

typedef struct {
    const char *tag;
    const char *score;
    const char *controls;
    const char *game_over;
    const char *exit_prefix;
    const char *exit_suffix;
} tetris_strings_t;

static const tetris_strings_t tetris_translations[] = {
    {
        .tag = "zh-CN",
        .score = "分数：",
        .controls = "操作说明:\n头向左：左移\n头向右：右移\n单击：旋转\n长按：快速下落\n抬头3秒退出",
        .game_over = "游戏结束，单击继续\n抬头3秒退出",
        .exit_prefix = "",
        .exit_suffix = "s后退出",
    },
    {
        .tag = "en",
        .score = "Score: ",
        .controls = "Instructions:\nHead Left: Move Left\nHead Right: Move Right\nClick: Rotate\nLong Press: Fast Drop\nLook up for 3 seconds to exit",
        .game_over = "Game Over, Click to continue\nLook up for 3 seconds to exit",
        .exit_prefix = "",
        .exit_suffix = "s to Exit",
    },
    {
        .tag = "ja",
        .score = "スコア: ",
        .controls = "操作説明:\n頭を左に向ける：左移動\n頭を右に向ける：右移動\nクリック：回転\n長押し：高速落下\n3秒間上を向いて終了",
        .game_over = "ゲームオーバー、クリックして続行\n3秒間上を向いて終了",
        .exit_prefix = "",
        .exit_suffix = "s後に終了",
    },
    {
        .tag = "ko",
        .score = "점수: ",
        .controls = "조작 설명:\n왼쪽으로 머리 돌리기: 왼쪽으로 이동\n오른쪽으로 머리 돌리기: 오른쪽으로 이동\n클릭: 회전\n길게 누르기: 빠르게 떨어뜨리기\n3초간 위를 보면 종료",
        .game_over = "게임 오버, 클릭하여 계속\n3초간 위를 보면 종료",
        .exit_prefix = "",
        .exit_suffix = "s 후 종료",
    },
    {
        .tag = "de",
        .score = "Punkte: ",
        .controls = "Bedienungsanleitung:\nKopf nach links: Nach links bewegen\nKopf nach rechts: Nach rechts bewegen\nKlicken: Drehen\nLanges Drücken: Schnelles Fallenlassen\n3 Sek. nach oben schauen: Beenden",
        .game_over = "Game Over, Klicken zum Fortsetzen\n3 Sek. nach oben schauen: Beenden",
        .exit_prefix = "",
        .exit_suffix = "s bis zum Beenden",
    },
    {
        .tag = "fr",
        .score = "Score : ",
        .controls = "Instructions :\nTête à gauche : Déplacer à gauche\nTête à droite : Déplacer à droite\nCliquez : Tourner\nAppui long : Chute rapide\nRegarder en haut pendant 3 s : quitter",
        .game_over = "Game Over, Cliquez pour continuer\nRegarder en haut pendant 3 s : quitter",
        .exit_prefix = "",
        .exit_suffix = "s avant de quitter",
    },
    {
        .tag = "es",
        .score = "Puntuación: ",
        .controls = "Instrucciones:\nCabeza a la izquierda: Mover a la izquierda\nCabeza a la derecha: Mover a la derecha\nClic: Rotar\nPulsación larga: Caída rápida\nMira hacia arriba durante 3 s: salir",
        .game_over = "Game Over, Haga clic para continuar\nMira hacia arriba durante 3 s: salir",
        .exit_prefix = "",
        .exit_suffix = "s para salir",
    },
    {
        .tag = "it",
        .score = "Punteggio: ",
        .controls = "Istruzioni:\nTesta a sinistra: Sposta a sinistra\nTesta a destra: Sposta a destra\nClicca: Ruota\nPressione prolungata: Caduta rapida\nGuarda in alto per 3 s: esci",
        .game_over = "Game Over, Clicca per continuare\nGuarda in alto per 3 s: esci",
        .exit_prefix = "",
        .exit_suffix = "s per uscire",
    },
    {
        .tag = "ru",
        .score = "Счет: ",
        .controls = "Инструкции:\nГолова влево: двигаться влево\nГолова вправо: двигаться вправо\nКлик: вращать\nДлительное нажатие: быстрое падение\nСмотреть вверх 3 секунды: выйти",
        .game_over = "Игра окончена, нажмите, чтобы продолжить\nСмотреть вверх 3 секунды: выйти",
        .exit_prefix = "",
        .exit_suffix = "s до выхода",
    },
    {
        .tag = "pt",
        .score = "Pontuação: ",
        .controls = "Instruções:\nCabeça para esquerda: Mover para esquerda\nCabeça para direita: Mover para direita\nClique: Girar\nPressão longa: Queda rápida\nOlhe para cima por 3 s: sair",
        .game_over = "Game Over, Clique para continuar\nOlhe para cima por 3 s: sair",
        .exit_prefix = "",
        .exit_suffix = "s para sair",
    },
    {
        .tag = "zh-TW",
        .score = "分數：",
        .controls = "操作說明:\n頭向左：左移\n頭向右：右移\n單擊：旋轉\n長按：快速下落\n抬頭3秒退出",
        .game_over = "遊戲結束，單擊繼續\n抬頭3秒退出",
        .exit_prefix = "",
        .exit_suffix = "s後退出",
    },
};

#endif
