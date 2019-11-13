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
    const monitors = dwmjs.getAllMonitors();
    alert(JSON.stringify(monitors)); // [{ id: '....' }, { id: '...' }]

    const windows = dwmjs.getAllWindows();
    alert(JSON.stringify(windows)); // [{ id: '....', { id: '...' } }]

    dwmjs.addEventListener('windowcreated', function (e) {
        const window = dwmjs.getWindowById(e.windowId);
    });

    dwmjs.exit(0 /* status code */);
});
```
