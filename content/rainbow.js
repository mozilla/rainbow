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

let EXPORTED_SYMBOLS = ["Rainbow"];

const Cc = Components.classes;
const Ci = Components.interfaces;

let Rainbow = {
    _input: null,
    _recording: false,

    _makePropertyBag: function(prop) {
        let bP = ["audio", "video"];
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

    recordToFile: function(prop, ctx) {
        if (Rainbow._recording)
            throw "Recording already in progress";

        // Make property bag
        let bag = Rainbow._makePropertyBag(prop);

        // Create a file to dump to
        let file = Cc["@mozilla.org/file/directory_service;1"].  
            getService(Ci.nsIProperties).get("TmpD", Ci.nsILocalFile);
        file.append("rainbow.ogg");
        file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

        // Create dummy HTML <input> element to create DOMFile
        let doc = ctx.canvas.ownerDocument;
        Rainbow._input = doc.createElement('input');
        Rainbow._input.type = 'file';
        Rainbow._input.mozSetFileNameArray([file.path], 1);

        // Start recording
        Cc["@labs.mozilla.com/media/recorder;1"].
            getService(Ci.IMediaRecorder).recordToFile(bag, ctx, file);
        
        Rainbow._recording = true;
    },

    recordToSocket: function(prop, ctx, sock) {
        if (Rainbow._recording)
            throw "Recording already in progress";

        let bag = Rainbow._makePropertyBag(prop);
        Cc["@labs.mozilla.com/media/recorder;1"].
            getService(Ci.IMediaRecorder).recordToSocket(bag, ctx, sock);
        
        Rainbow._recording = true;
    },

    stop: function() {
        if (!Rainbow._recording)
            throw "No recording in progress";

        Cc["@labs.mozilla.com/media/recorder;1"].
            getService(Ci.IMediaRecorder).stop();
        Rainbow._recording = false;

        if (Rainbow._input) {
            let ret = Rainbow._input;
            Rainbow._input = null;
            return ret;
        }
    }
};

