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
        "操作说明:\n头部/方向键左右：移动\n方向键上/单击：旋转\n方向键下：下落一格\n长按：快速下落\n抬头：退出",
        "游戏结束，单击继续\n抬头退出", "", "s后退出"
    },
    {
        "en", "Score: ",
        "Instructions:\nHead/Left/Right: Move\nUp/Click: Rotate\nDown: Drop One Row\nLong Press: Fast Drop\nLook Up: Exit",
        "Game Over, Click to continue\nLook Up: Exit", "", "s to Exit"
    },
    {
        "ja", "スコア: ",
        "操作説明:\n頭/左右キー: 移動\n上キー/クリック: 回転\n下キー: 1段落下\n長押し: 高速落下\n上を向く: 終了",
        "ゲームオーバー、クリックして続行\n上を向く：終了", "", "s後に終了"
    },
    {
        "ko", "점수: ",
        "조작법:\n머리/좌우 키: 이동\n위 키/클릭: 회전\n아래 키: 한 칸 하강\n길게 누르기: 빠른 하강\n위 보기: 종료",
        "게임 오버, 클릭하여 계속\n위로 보기: 종료", "", "s 후 종료"
    },
    {
        "de", "Punkte: ",
        "Steuerung:\nKopf/Links/Rechts: Bewegen\nOben/Klick: Drehen\nUnten: Eine Reihe fallen\nHalten: Schnell fallen\nHochsehen: Ende",
        "Game Over, Klicken zum Fortsetzen\nNach oben schauen: Beenden", "", "s bis zum Beenden"
    },
    {
        "fr", "Score : ",
        "Commandes :\nTête/Gauche/Droite : déplacer\nHaut/Clic : tourner\nBas : descendre d'une ligne\nAppui long : chute rapide\nLever la tête : quitter",
        "Game Over, Cliquez pour continuer\nRegarder en haut : Quitter", "", "s avant de quitter"
    },
    {
        "es", "Puntuación: ",
        "Controles:\nCabeza/Izq./Der.: mover\nArriba/Clic: rotar\nAbajo: bajar una fila\nPulsación larga: caída rápida\nMirar arriba: salir",
        "Game Over, Haga clic para continuar\nMirar hacia arriba: Salir", "", "s para salir"
    },
    {
        "it", "Punteggio: ",
        "Comandi:\nTesta/Sinistra/Destra: muovi\nSu/Clic: ruota\nGiù: scendi una riga\nPressione lunga: caduta rapida\nGuarda su: esci",
        "Game Over, Clicca per continuare\nGuarda in alto: Esci", "", "s per uscire"
    },
    {
        "ru", "Счет: ",
        "Управление:\nГолова/Влево/Вправо: двигать\nВверх/Клик: вращать\nВниз: опустить на ряд\nУдержание: быстрое падение\nСмотреть вверх: выход",
        "Игра окончена, нажмите, чтобы продолжить\nСмотреть вверх: выйти", "", "s до выхода"
    },
    {
        "pt", "Pontuação: ",
        "Controles:\nCabeça/Esq./Dir.: mover\nCima/Clique: girar\nBaixo: descer uma linha\nPressão longa: queda rápida\nOlhar para cima: sair",
        "Game Over, Clique para continuar\nOlhar para cima: Sair", "", "s para sair"
    },
    {
        "zh-TW", "分數：",
        "操作說明:\n頭部/方向鍵左右：移動\n方向鍵上/單擊：旋轉\n方向鍵下：下落一格\n長按：快速下落\n抬頭：退出",
        "遊戲結束，單擊繼續\n抬頭退出", "", "s後退出"
    },
};

#endif
