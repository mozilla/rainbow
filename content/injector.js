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
const SCRIPT_TO_INJECT_URI = "resource://rainbow/content/injected.js";

// DOUBLE RAINBOW!!
var rainbow = new Rainbow();
var observer = Components.classes["@mozilla.org/observer-service;1"].
    getService(Components.interfaces.nsIObserverService);

// Load up what we need to inject
function getInjected() {
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
    return toInject;
}

var RainbowObserver = {
    QueryInterface: XPCOMUtils.generateQI([
        Components.interfaces.nsIObserver,
    ]),

    observe: function(subject, topic, data) {
        // Check if XPCOM component is loaded, don't bother injecting if
        // absent (most likely FF is in 64-bit mode)
        try {
            let i = Components.classes["@labs.mozilla.com/media/recorder;1"].
                getService(Components.interfaces.IMediaRecorder);
        } catch(e) {
            Components.utils.reportError(
                "Rainbow could not load component, are you in " +
                "32-bit mode?"
            );
            return;
        }
        
        // We inject on all domains, permission checks are performed
        // on-call in rainbow.js
        let sandbox = new Components.utils.Sandbox(
            Components.classes["@mozilla.org/systemprincipal;1"].
               createInstance(Components.interfaces.nsIPrincipal)
        );
        sandbox.window = subject.wrappedJSObject;

        sandbox.importFunction(function(loc, prop, ctx, obs) {
            rainbow._verifyPermission(window, loc, function(allowed) {
                if (allowed) rainbow.beginSession(prop, ctx, obs);
                else throw "Permission denied";
            });
        }, "sessStart");
        sandbox.importFunction(function(loc) {
            rainbow._verifyPermission(window, loc, function(allowed) {
                if (allowed) rainbow.beginRecording();
                else throw "Permission denied";
            });
        }, "recStart");
        sandbox.importFunction(function(loc) {
            let ret;
            rainbow._verifyPermission(window, loc, function(allowed) {
                if (allowed) ret = rainbow.endRecording();
                else throw "Permission denied";
            });
            return ret;
        }, "recStop");
        sandbox.importFunction(function(loc) {
            rainbow._verifyPermission(window, loc, function(allowed) {
                if (allowed) rainbow.endSession();
                else throw "Permission denied";
            });
        }, "sessStop");
        sandbox.importFunction(function(loc, isFile) {
            let ret;
            rainbow._verifyPermission(window, loc, function(allowed) {
                if (allowed) ret = rainbow.fetchImage(isFile);
                else throw "Permission denied";
            });
            return ret;
        }, "fetchImg");
        
        let toInject = getInjected();
        Components.utils.evalInSandbox(
            toInject, sandbox, "1.8", this.SCRIPT_TO_INJECT_URI, 1
        );
    }
};

window.addEventListener("load", function() {
    observer.addObserver(RainbowObserver, "content-document-global-created", false);
}, false);
window.addEventListener("unload", function() {
    observer.removeObserver(RainbowObserver, "content-document-global-created");
}, false);
