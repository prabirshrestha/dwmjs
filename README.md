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
    alert(JSON.stringify(monitors)); // [{ id: '....' }, { id: '...' }]

    const windows = dwmjs.getWindows();
    alert(JSON.stringify(windows)); // [{ id: '....', { id: '...' } }]

    var created = false;
    dwmjs.addEventListener('windowcreate', function (e) {
        if (created) { return; }
        created = true;
        const window = dwmjs.getWindowById(e.windowId);
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
    });

    dwmjs.exit(0 /* status code */);
});
```
