#ifndef JET_RUNNER_TRANSLATIONS_H
#define JET_RUNNER_TRANSLATIONS_H

typedef struct {
    const char *tag;
    const char *title;
    const char *score;
    const char *controls;
    const char *collision;
    const char *restart;
} jet_runner_strings_t;

static const jet_runner_strings_t jet_runner_translations[] = {
    {"zh-CN", "喷气跑酷", "分数：", "头动或配件上/下键：飞行 · 长按退出",
     "发生碰撞！分数：", "\n单击重新开始"},
    {"en", "Jet Runner", "Score: ", "Head/accessory up/down: fly · Hold to exit",
     "Collision! Score: ", "\nClick to restart"},
    {"ja", "ジェットランナー", "スコア: ", "頭/アクセサリ上下: 飛行 · 長押しで終了",
     "衝突！スコア: ", "\nクリックして再スタート"},
    {"ko", "제트 러너", "점수: ", "머리/액세서리 위아래: 비행 · 길게 눌러 종료",
     "충돌! 점수: ", "\n클릭하여 재시작"},
    {"de", "Jet Runner", "Punkte: ", "Kopf/Zubehör auf/ab: fliegen · Halten: beenden",
     "Kollision! Punkte: ", "\nKlicken zum Neustart"},
    {"fr", "Jet Runner", "Score : ", "Tête/accessoire haut/bas : voler · Maintenir : quitter",
     "Collision ! Score : ", "\nCliquez pour rejouer"},
    {"es", "Jet Runner", "Puntuación: ", "Cabeza/accesorio arriba/abajo: volar · Mantén: salir",
     "¡Colisión! Puntuación: ", "\nHaz clic para reiniciar"},
    {"it", "Jet Runner", "Punteggio: ", "Testa/accessorio su/giù: vola · Tieni premuto: esci",
     "Collisione! Punteggio: ", "\nClicca per riavviare"},
    {"ru", "Джет-раннер", "Счет: ", "Голова/аксессуар вверх-вниз: полёт · Удерживать: выход",
     "Столкновение! Счёт: ", "\nНажмите для перезапуска"},
    {"pt", "Jet Runner", "Pontuação: ", "Cabeça/acessório cima/baixo: voar · Segure: sair",
     "Colisão! Pontuação: ", "\nClique para reiniciar"},
    {"zh-TW", "噴氣跑酷", "分數：", "頭動或配件上/下鍵：飛行 · 長按退出",
     "發生碰撞！分數：", "\n單擊重新開始"},
};

#endif
