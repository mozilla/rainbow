/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is People.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
 *   Justin Dolske <dolske@mozilla.com>
 *   Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://rainbow/content/rainbow.js");

const PREFNAME = "allowedDomains";
const DEFAULT_DOMAINS = ["http://localhost"];

let RainbowInjector = {
    SCRIPT_TO_INJECT_URI: "resource://rainbow/content/injected.js",
    get _toInject() {
        delete this._toInject;

        let ioSvc = Components.classes["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
        let uri = ioSvc.newURI(this.SCRIPT_TO_INJECT_URI, null, null).
            QueryInterface(Components.interfaces.nsIFileURL);
        let inputStream =
            Components.classes["@mozilla.org/network/file-input-stream;1"].
                createInstance(Components.interfaces.nsIFileInputStream);
    
        inputStream.init(uri.file, 0x01, -1, null);
        let lineStream = inputStream.
            QueryInterface(Components.interfaces.nsILineInputStream);

        let line = { value: "" }, hasMore, toInject = "";
        do {
            hasMore = lineStream.readLine(line);
            toInject += line.value + "\n";
        } while (hasMore);
        lineStream.close();

        return this._toInject = toInject;
    },

    inject: function(win) {
        let sandbox = new Components.utils.Sandbox(
            Components.classes["@mozilla.org/systemprincipal;1"].
               createInstance(Components.interfaces.nsIPrincipal)
        );
        sandbox.window = win.wrappedJSObject;

        sandbox.importFunction(Rainbow.stop, "recStop");
        sandbox.importFunction(Rainbow.recordToFile, "recFStart");
        sandbox.importFunction(Rainbow.recordToSocket, "recSStart");
        
        Components.utils.evalInSandbox(
            this._toInject, sandbox, "1.8", this.SCRIPT_TO_INJECT_URI, 1
        );
    }
};

let RainbowObserver = {
    get _observer() {
        delete this._observer;
        return this._observer = 
            Components.classes["@mozilla.org/observer-service;1"].
                getService(Components.interfaces.nsIObserverService);
    },

    get _prefs() {
        delete this._prefs;
        return this._prefs =
            Components.classes["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService).getBranch("extensions.rainbow.").
                    QueryInterface(Ci.nsIPrefBranch2);
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
        if (this._prefs.getPrefType(PREFNAME) ==
            Ci.nsIPrefBranch.PREF_INVALID) {
            this._prefs.setCharPref(PREFNAME, JSON.stringify(DEFAULT_DOMAINS));
        }

        let domains = JSON.parse(this._prefs.getCharPref(PREFNAME));
        domains.forEach(function(domain) {
            if (data === domain)
                RainbowInjector.inject(subject);
        });
    }
};

window.addEventListener("load", function() RainbowObserver.onLoad(), false);
window.addEventListener("unload", function() RainbowObserver.onUnload(), false);

