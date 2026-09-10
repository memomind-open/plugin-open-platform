const translations = Object.freeze({
  en: Object.freeze({
    'language.switch': '中文',
    'app.eyebrow': 'MEMOMIND DISPLAY SDK',
    'app.title': 'Display Control Lab',
    'app.summary': 'Control glasses display power and optical levels.',
    'runtime.host': 'Host',
    'runtime.glasses': 'Glasses',
    'runtime.mode': 'Mode',
    'runtime.connecting': 'Connecting',
    'runtime.connected': 'Connected',
    'runtime.disconnected': 'Disconnected',
    'runtime.physical': 'Physical device',
    'runtime.desktop': 'Studio preview',
    'runtime.browser': 'Browser preview',
    'notice.initializing': 'Waiting for the plugin Host...',
    'notice.ready': 'Display controls are ready.',
    'notice.sent': 'Setting applied by the glasses plugin.',
    'notice.restoring': 'Restoring the initial display settings. This may take a few seconds...',
    'notice.woke': 'The display was turned on by a glasses input.',
    'notice.preview': 'Preview updated. Optical effects require physical glasses.',
    'notice.unavailable': 'Pair and start the Display Control Lab glasses plugin.',
    'notice.failed': 'Control failed: {error}',
    'panel.screen': 'Screen Power',
    'panel.screenHint': 'Turn off only the display, not the glasses.',
    'screen.on': 'Turn On',
    'screen.off': 'Turn Off',
    'screen.current': 'Current: {value}',
    'screen.onValue': 'On',
    'screen.offValue': 'Off',
    'screen.previewValue': '{value} (preview)',
    'control.brightness': 'Brightness',
    'control.brightnessHint': 'Level 1–10',
    'control.height': 'Display Height',
    'control.heightHint': 'Vertical optical level 0–8',
    'control.distance': 'Display Distance',
    'control.distanceHint': 'Optical distance level 0–8',
    'action.restore': 'Restore Initial Settings',
    'action.restoring': 'Restoring...',
    'safety.title': 'Safety recovery',
    'safety.body': 'If the display is off, any glasses or accessory button and the head-raise gesture turn it on. Holding the primary button while the display is on exits the demo.',
    'footer': 'Studio validates commands and values. Verify brightness, height, distance, and power on physical glasses.',
  }),
  zh: Object.freeze({
    'language.switch': 'EN',
    'app.eyebrow': 'MEMOMIND 显示 SDK',
    'app.title': '屏幕控制实验室',
    'app.summary': '控制眼镜屏幕电源和光学显示等级。',
    'runtime.host': '宿主',
    'runtime.glasses': '眼镜',
    'runtime.mode': '模式',
    'runtime.connecting': '连接中',
    'runtime.connected': '已连接',
    'runtime.disconnected': '未连接',
    'runtime.physical': '实体眼镜',
    'runtime.desktop': 'Studio 预览',
    'runtime.browser': '浏览器预览',
    'notice.initializing': '正在等待插件宿主……',
    'notice.ready': '屏幕控制已就绪。',
    'notice.sent': '眼镜插件已应用设置。',
    'notice.restoring': '正在恢复初始显示设置，可能需要几秒……',
    'notice.woke': '眼镜端输入已触发亮屏。',
    'notice.preview': '预览状态已更新，光学效果需要实体眼镜验证。',
    'notice.unavailable': '请配对并启动屏幕控制实验室眼镜插件。',
    'notice.failed': '控制失败：{error}',
    'panel.screen': '屏幕电源',
    'panel.screenHint': '只关闭显示屏，不会关闭眼镜电源。',
    'screen.on': '开屏',
    'screen.off': '关屏',
    'screen.current': '当前：{value}',
    'screen.onValue': '开启',
    'screen.offValue': '关闭',
    'screen.previewValue': '{value}（预览）',
    'control.brightness': '亮度',
    'control.brightnessHint': '等级 1–10',
    'control.height': '显示高度',
    'control.heightHint': '垂直光学等级 0–8',
    'control.distance': '显示距离',
    'control.distanceHint': '光学距离等级 0–8',
    'action.restore': '恢复初始设置',
    'action.restoring': '正在恢复……',
    'safety.title': '安全恢复',
    'safety.body': '屏幕关闭时，任意眼镜或配件按键以及抬头动作都会亮屏；屏幕亮起时长按主键可退出演示。',
    'footer': 'Studio 用于验证命令和数值，亮度、高度、距离和开关屏效果请在实体眼镜验证。',
  }),
});

export function createI18n(environment = globalThis) {
  let language = /^zh\b/iu.test(environment.navigator?.language ?? '') ? 'zh' : 'en';

  const t = (key, variables = {}) => {
    const template = translations[language][key] ?? translations.en[key] ?? key;
    return Object.entries(variables).reduce(
      (text, [name, value]) => text.replaceAll(`{${name}}`, String(value)),
      template,
    );
  };

  const apply = (root = environment.document) => {
    if (!root) return;
    root.documentElement?.setAttribute('lang', language === 'zh' ? 'zh-CN' : 'en');
    root.querySelectorAll?.('[data-i18n]').forEach((node) => {
      node.textContent = t(node.dataset.i18n);
    });
  };

  return {
    get language() { return language; },
    t,
    apply,
    toggle() {
      language = language === 'en' ? 'zh' : 'en';
      apply();
      return language;
    },
  };
}
