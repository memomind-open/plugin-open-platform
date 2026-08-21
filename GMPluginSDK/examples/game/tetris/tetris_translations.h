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
        "zh-CN", "分数：",
        "操作说明:\n头向左：左移\n头向右：右移\n单击：旋转\n长按：快速下落\n抬头退出",
        "游戏结束，单击继续\n抬头退出", "", "s后退出"
    },
    {
        "en", "Score: ",
        "Instructions:\nHead Left: Move Left\nHead Right: Move Right\nClick: Rotate\nLong Press: Fast Drop\nLook Up: Exit",
        "Game Over, Click to continue\nLook Up: Exit", "", "s to Exit"
    },
    {
        "ja", "スコア: ",
        "操作説明:\n頭を左に向ける：左移動\n頭を右に向ける：右移動\nクリック：回転\n長押し：高速落下\n上を向く：終了",
        "ゲームオーバー、クリックして続行\n上を向く：終了", "", "s後に終了"
    },
    {
        "ko", "점수: ",
        "조작 설명:\n왼쪽으로 머리 돌리기: 왼쪽으로 이동\n오른쪽으로 머리 돌리기: 오른쪽으로 이동\n클릭: 회전\n길게 누르기: 빠르게 떨어뜨리기\n위로 보기: 종료",
        "게임 오버, 클릭하여 계속\n위로 보기: 종료", "", "s 후 종료"
    },
    {
        "de", "Punkte: ",
        "Bedienungsanleitung:\nKopf nach links: Nach links bewegen\nKopf nach rechts: Nach rechts bewegen\nKlicken: Drehen\nLanges Drücken: Schnelles Fallenlassen\nNach oben schauen: Beenden",
        "Game Over, Klicken zum Fortsetzen\nNach oben schauen: Beenden", "", "s bis zum Beenden"
    },
    {
        "fr", "Score : ",
        "Instructions :\nTête à gauche : Déplacer à gauche\nTête à droite : Déplacer à droite\nCliquez : Tourner\nAppui long : Chute rapide\nRegarder en haut : Quitter",
        "Game Over, Cliquez pour continuer\nRegarder en haut : Quitter", "", "s avant de quitter"
    },
    {
        "es", "Puntuación: ",
        "Instrucciones:\nCabeza a la izquierda: Mover a la izquierda\nCabeza a la derecha: Mover a la derecha\nClic: Rotar\nPulsación larga: Caída rápida\nMirar hacia arriba: Salir",
        "Game Over, Haga clic para continuar\nMirar hacia arriba: Salir", "", "s para salir"
    },
    {
        "it", "Punteggio: ",
        "Istruzioni:\nTesta a sinistra: Sposta a sinistra\nTesta a destra: Sposta a destra\nClicca: Ruota\nPressione prolungata: Caduta rapida\nGuarda in alto: Esci",
        "Game Over, Clicca per continuare\nGuarda in alto: Esci", "", "s per uscire"
    },
    {
        "ru", "Счет: ",
        "Инструкции:\nГолова влево: двигаться влево\nГолова вправо: двигаться вправо\nКлик: вращать\nДлительное нажатие: быстрое падение\nСмотреть вверх: выйти",
        "Игра окончена, нажмите, чтобы продолжить\nСмотреть вверх: выйти", "", "s до выхода"
    },
    {
        "pt", "Pontuação: ",
        "Instruções:\nCabeça para esquerda: Mover para esquerda\nCabeça para direita: Mover para direita\nClique: Girar\nPressão longa: Queda rápida\nOlhar para cima: Sair",
        "Game Over, Clique para continuar\nOlhar para cima: Sair", "", "s para sair"
    },
    {
        "zh-TW", "分數：",
        "操作說明:\n頭向左：左移\n頭向右：右移\n單擊：旋轉\n長按：快速下落\n抬頭退出",
        "遊戲結束，單擊繼續\n抬頭退出", "", "s後退出"
    },
};

#endif
