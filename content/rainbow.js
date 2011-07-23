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
 * The Original Code is Rainbow.
 *
 * The Initial Developer of the Original Code is Mozilla Labs.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Anant Narayanan <anant@kix.in>
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

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const PREFNAME = "allowedDomains";
const DEFAULT_DOMAINS = [
    "http://localhost",
    "http://mozilla.github.com"
];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function Rainbow() {
    this._input = null;
    this._context = null;
    this._session = false;
    this._recording = false;
}
Rainbow.prototype = {
    get _prefs() {
        delete this._prefs;
        return this._prefs =
            Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService).getBranch("extensions.rainbow.").
                    QueryInterface(Ci.nsIPrefBranch2);
    },
    
    get _perms() {
        delete this._perms;
        return this._perms = 
            Cc["@mozilla.org/permissionmanager;1"].
                getService(Ci.nsIPermissionManager);
    },
    
    get _rainbow() {
        delete this._recorder;
        return this._recorder =
            Cc["@labs.mozilla.com/media/recorder;1"].
                getService(Ci.IMediaRecorder)
    },
    
    _makeURI: function(url) {
        let iosvc = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
        return iosvc.newURI(url, null, null);
    },
    
    _makePropertyBag: function(prop) {
        let bP = ["audio", "video", "source"];
        let iP = ["width", "height", "channels", "rate"];
        let dP = ["quality"];

        let bag = Cc["@mozilla.org/hash-property-bag;1"].
            createInstance(Ci.nsIWritablePropertyBag2);
        
        bP.forEach(function(p) {
            if (p in prop)
                bag.setPropertyAsBool(p, prop[p]);
        });
        iP.forEach(function(p) {
            if (p in prop)
                bag.setPropertyAsUint32(p, prop[p]);
        });
        dP.forEach(function(p) {
            if (p in prop)
                bag.setPropertyAsDouble(p, prop[p]);
        });

        return bag;
    },

    _verifyPermission: function(win, loc, cb) {
        let location = loc.protocol + "//" + loc.hostname;
        if (loc.port) location += ":" + loc.port;
        
        // Domains in the preference are always allowed
        let found = false;
        if (this._prefs.getPrefType(PREFNAME) ==
            Ci.nsIPrefBranch.PREF_INVALID) {
            this._prefs.setCharPref(PREFNAME, JSON.stringify(DEFAULT_DOMAINS));
        }
        let domains = JSON.parse(this._prefs.getCharPref(PREFNAME));
        domains.forEach(function(domain) {
            if (location == domain) { cb(true); found = true; }
        });
        if (found)
            return;
            
        // If domain not found in preference, check permission manager
        let self = this;
        let uri = this._makeURI(location);
        switch (this._perms.testExactPermission(uri, "rainbow")) {
            case Ci.nsIPermissionManager.ALLOW_ACTION:
                cb(true); break;
            case Ci.nsIPermissionManager.DENY_ACTION:
                cb(false); break;
            case Ci.nsIPermissionManager.UNKNOWN_ACTION:
                // Ask user
                let acceptButton = {
                    label: "Yes", accessKey: "y",
                    callback: function() {
                        self._perms.add(uri, "rainbow", Ci.nsIPermissionManager.ALLOW_ACTION);
                        cb(true);
                    }
                };
                let declineButton = {
                    label: "No", accessKey: "n",
                    callback: function() {
                        self._perms.add(uri, "rainbow", Ci.nsIPermissionManager.DENY_ACTION);
                        cb(false);
                    }
                };
                
                let message =
                    "This website is requesting access to your webcam " +
                    "and microphone. Do you wish to allow it?";
                
                let ret = win.PopupNotifications.show(
                    win.gBrowser.selectedBrowser,
                    "rainbow-access-request",
                    message, null, acceptButton, [declineButton], {
                        "persistence": 1,
                        "persistWhileVisible": true,
                        "eventCallback": function(state) {
                            // Dismissing prompt implies immediate denial
                            // but we do not persist the choice
                            if (state == "dismissed") {
                                cb(false); ret.remove();
                            }
                        }
                    }
                );
        }
    },
    
    beginSession: function(prop, ctx, audio, sampler, obs) {
        if (this._session)
            throw "Session already in progress";

        // Make property bag
        let bag = this._makePropertyBag(prop);
        
        // Make sure observer is setup correctly, if none provided, ignore
        if (obs) this._observer = obs;
        else this._observer = function() {};
        
        // Create dummy HTML <input> element to create DOMFile(s)
        this._context = ctx;

        let doc;
        if (this._context)
            doc = this._context.canvas.ownerDocument;
        else if (audio)
            doc = audio.ownerDocument;
        
        this._input = doc.createElement('input');
        this._input.type = 'file';
        
        // Start rainbow session
        if (!sampler)
            sampler = function() {};
        this._rainbow.beginSession(bag, ctx, audio, function(sample) {
            sampler(sample.frames);
        }, this._observer);
        this._session = true;
    },
    
    fetchImage: function(isFile) {
        if (!this._context)
            throw "Session is missing a canvas context!";
            
        // fetch data url from canvas
        let data = this._context.canvas.toDataURL("image/png", "");
        if (!isFile) {
            return data;
        }
        
        // create URIs of the source and targets
        let file = Cc["@mozilla.org/file/directory_service;1"].  
            getService(Ci.nsIProperties).get("TmpD", Ci.nsILocalFile);
        file.append("rainbow.png");
        file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
        let io = Cc["@mozilla.org/network/io-service;1"]
                 .getService(Ci.nsIIOService);
        let source = io.newURI(data, "UTF8", null);
        let target = io.newFileURI(file);

        // prepare to save the canvas data
        let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                      .createInstance(Ci.nsIWebBrowserPersist);
        persist.persistFlags =
            Ci.nsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
        persist.persistFlags |=
            Ci.nsIWebBrowserPersist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

        // save the canvas data to the file
        persist.saveURI(source, null, null, null, null, file);
        
        if (!this._input) {
            this._input =
                this._context.canvas.ownerDocument.createElement('input');
            this._input.type = 'file';
        }
        this._input.mozSetFileNameArray([file.path], 1);
        return this._input;
    },
    
    beginRecording: function() {
        if (!this._session)
            throw "Session is not in progress";
        if (this._recording)
            throw "Recording in progress";
            
        // Create a file to dump to
        let file = Cc["@mozilla.org/file/directory_service;1"].  
            getService(Ci.nsIProperties).get("TmpD", Ci.nsILocalFile);
        file.append("rainbow.ogg");
        file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

        this._input.mozSetFileNameArray([file.path], 1);
        this._rainbow.beginRecording(file);
        this._recording = true;
    },
    
    endRecording: function() {
        if (!this._recording)
            throw "Recording is not in progress";
        
        this._rainbow.endRecording();
        this._recording = false;
        if (this._input) {
            let ret = this._input;
            this._input = null;
            this._observer("record-finished", ret);
        }
    },
    
    endSession: function() {
        if (!this._session)
            throw "No session in progress";
        this._rainbow.endSession();
        this._session = false;
    }
};

var EXPORTED_SYMBOLS = ["Rainbow"];
