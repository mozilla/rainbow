Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const ALLOWED_DOMAINS = [
    "http://localhost",
    "https://localhost"
];

let RainbowInjector = {
    get _media() {
        delete this._media;
        return this._media =
            Components.classes["@labs.mozilla.com/media/recorder;1"].
                getService(Components.interfaces.IMediaRecorder);
    },

    get _toInject() {
        delete this._toInject;
        return this._toInject = 
            "if (window && window.navigator) {\n" +
            "   if (!window.navigator.service)\n" +
            "       window.navigator.service = {};\n" +
            "   window.navigator.service.media = {\n" +
            "       start: function(audio, video, canvas) {\n" +
            "           return recStart(audio, video, canvas);\n" +
            "       },\n" +
            "       stop: function() {\n" +
            "           return recStop();\n" +
            "       }\n" +
            "   };\n" +
            "}\n";
    },

    inject: function(win) {
        let sandbox = new Components.utils.Sandbox(win);
        sandbox.importFunction(this._media.start, "recStart");
        sandbox.importFunction(this._media.stop, "recStop");
        sandbox.window = win.wrappedJSObject;

        dump(Components.utils.evalInSandbox(
            this._toInject, sandbox, "1.8", "injected.js", 1
        ));
    }
};

let RainbowObserver = {
    get _observer() {
        delete this._observer;
        return this._observer = 
            Components.classes["@mozilla.org/observer-service;1"].
                getService(Components.interfaces.nsIObserverService);
    },

    QueryInterface: XPCOMUtils.generateQI([
        Components.interfaces.nsIObserver,
    ]),

    onLoad: function() {
        this._observer.addObserver(
            this, "content-document-global-created", false
        );
    },

    onUnload: function() {
        this._observer.removeObserver(
            this, "content-document-global-created"
        );
    },

    observe: function(subject, topic, data) {
        ALLOWED_DOMAINS.forEach(function(domain) {
            if (data === domain)
                RainbowInjector.inject(subject);
        });
    }
};

window.addEventListener("load", function() RainbowObserver.onLoad(), false);
window.addEventListener("unload", function() RainbowObserver.onUnload(), false);
