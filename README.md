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
    alert(JSON.stringify(monitors)); // ['1','2']
    const monitor = dwmjs.getMonitorById(monitors[0]); // { id: '1', x: 0, y: 0, height: 1920, width: 1080 }

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

    dwmjs.exit(0 /* status code */);
});
```

## TODO

- [ ] Add support for custom bar - https://github.com/LemonBoy/bar
```javascript
var bar = new Bar({ x: 0, y: 0, width: 1080, height: 20 });
bar.draw('.....');
```
- [ ] Implement initial tiling manager
- [ ] detect screen resolution changes
