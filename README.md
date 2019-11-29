# Dynamic Window Manager with JavaScript configuration
Configure windows with JavaScript.

# Building

## Windows

Install [scoop](https://scoop.sh/), `make` and `llvm clang v9.0.0`.
```cmd
scoop install make llvm
make
```

# Example config

Config is loaded from `~/.dwm.js`. For windows this will be located in `%USERPROFILE\.dwm.js`.

```javascript
dwmjs.addEventListener('load', function () {
    const monitors = dwmjs.getMonitors();
    alert(JSON.stringify(monitors)); // ['id1', 'id2']
    alert(JSON.stringify(monitors[0])); // { id: 'id1', height: 1920, width: 1080 }

    const windows = dwmjs.getWindows();
    alert(JSON.stringify(windows)); // [123,234]

    var created = false;
    dwmjs.addEventListener('windowcreate', function (e) {
        if (created) { return; }
        created = true;
        const window = dwmjs.getWindowById(e.windowId);
        dwmjs.closeWindow(e.windowId);
    });

    var closed = false;
    dwmjs.addEventListener('windowclose', function (e) {
        if (closed) { return; }
        closed = true;
        const window = dwmjs.getWindowById(e.windowId);
    });

    var focused = false;
    dwmjs.addEventListener('windowfocusin', function (e) {
        if (focused) { return; }
        focused = true;
        const window = dwmjs.getWindowById(e.windowId);
        if (window.className == 'Vim') {
            dwmjs.setWindowAttributes(window.id, {
                borderBarVisibility: 'hidden', // 'hidden' | 'visible'
                visibility: 'hidden' // 'hidden' | 'visible'
            })
        }
    });

    var bar = new dwmjs.Bar({
        x: 0,
        y: 0,
        height: 16,
        width: 200,
        font: 'Consolas',
        underlineColor: '#dddddd',
        underlineWidth: 1,
        backgroundColor: '#ffffff',
        foregroundColor: '#cccccc',
    });

    var barAttributes = bar.getAttributes();

    dwmjs.exit(0 /* status code */);
});
```
