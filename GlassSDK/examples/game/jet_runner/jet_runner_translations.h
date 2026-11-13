#ifndef JET_RUNNER_TRANSLATIONS_H
#define JET_RUNNER_TRANSLATIONS_H

typedef struct {
    const char *tag;
    const char *hold_exit;
    const char *title;
    const char *score;
    const char *controls;
    const char *collision;
    const char *restart;
} jet_runner_strings_t;

static const jet_runner_strings_t jet_runner_translations[] = {
    {
        .tag = "zh-CN",
        .hold_exit = "按住2s结束",
        .title = "喷气跑酷",
        .score = "分数：",
        .controls = "抬头上升 · 低头下降 · 长按退出",
        .collision = "发生碰撞！分数：",
        .restart = "\n单击重新开始",
    },
    {
        .tag = "en",
        .hold_exit = "Hold for 2s to exit",
        .title = "Jet Runner",
        .score = "Score: ",
        .controls = "Look up/down to fly · Hold to exit",
        .collision = "Collision! Score: ",
        .restart = "\nClick to restart",
    },
    {
        .tag = "ja",
        .hold_exit = "2秒間長押しで終了",
        .title = "ジェットランナー",
        .score = "スコア: ",
        .controls = "上/下を向いて飛行 · 長押しで終了",
        .collision = "衝突！スコア: ",
        .restart = "\nクリックして再スタート",
    },
    {
        .tag = "ko",
        .hold_exit = "2초 동안 길게 눌러 종료",
        .title = "제트 러너",
        .score = "점수: ",
        .controls = "위/아래 보기: 비행 · 길게 눌러 종료",
        .collision = "충돌! 점수: ",
        .restart = "\n클릭하여 재시작",
    },
    {
        .tag = "de",
        .hold_exit = "2 s gedrückt halten, um zu beenden",
        .title = "Jet Runner",
        .score = "Punkte: ",
        .controls = "Auf/ab blicken: fliegen · Halten: beenden",
        .collision = "Kollision! Punkte: ",
        .restart = "\nKlicken zum Neustart",
    },
    {
        .tag = "fr",
        .hold_exit = "Maintenez 2 s pour quitter",
        .title = "Jet Runner",
        .score = "Score : ",
        .controls = "Regard haut/bas : voler · Maintenir : quitter",
        .collision = "Collision ! Score : ",
        .restart = "\nCliquez pour rejouer",
    },
    {
        .tag = "es",
        .hold_exit = "Mantén pulsado 2 s para salir",
        .title = "Jet Runner",
        .score = "Puntuación: ",
        .controls = "Mira arriba/abajo: volar · Mantén: salir",
        .collision = "¡Colisión! Puntuación: ",
        .restart = "\nHaz clic para reiniciar",
    },
    {
        .tag = "it",
        .hold_exit = "Tieni premuto 2 s per uscire",
        .title = "Jet Runner",
        .score = "Punteggio: ",
        .controls = "Guarda su/giù: vola · Tieni premuto: esci",
        .collision = "Collisione! Punteggio: ",
        .restart = "\nClicca per riavviare",
    },
    {
        .tag = "ru",
        .hold_exit = "Удерживайте 2 с, чтобы выйти",
        .title = "Джет-раннер",
        .score = "Счет: ",
        .controls = "Взгляд вверх/вниз: полёт · Удерживать: выход",
        .collision = "Столкновение! Счёт: ",
        .restart = "\nНажмите для перезапуска",
    },
    {
        .tag = "pt",
        .hold_exit = "Segure por 2 s para sair",
        .title = "Jet Runner",
        .score = "Pontuação: ",
        .controls = "Olhe para cima/baixo: voar · Segure: sair",
        .collision = "Colisão! Pontuação: ",
        .restart = "\nClique para reiniciar",
    },
    {
        .tag = "zh-TW",
        .hold_exit = "按住2s結束",
        .title = "噴氣跑酷",
        .score = "分數：",
        .controls = "抬頭上升 · 低頭下降 · 長按退出",
        .collision = "發生碰撞！分數：",
        .restart = "\n單擊重新開始",
    },
};

#endif
