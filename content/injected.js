/* 
 * Wrap IMediaRecorder in HTML5 device like API. Not quite there, but close.
 * http://www.whatwg.org/specs/web-apps/current-work/complete/commands.html#devices
 */
function StreamRecorder(audio, video, canvas)
{
    /* IMediaRecorder.start returns us a path which we then need to convert
     * to a an nsIDOMFile. Currently, this is only possible by going via
     * an HTMLInputElement
     */
    let path = recStart(audio, video, canvas);

    let inpe = window.document.createElement('input');
    inpe.type = 'file';
    inpe.mozSetFileNameArray(path, 1);

    this._file = inpe.files.item(0);
    return this;    
}
StreamRecorder.prototype.stop = function()
{
    recStop();
    return this._file;
}

/*
 * Expose media API just like the Mozilla Labs: Contacts addon
 */
if (window && window.navigator) {
    if (!window.navigator.service)
        window.navigator.service = {};

    window.navigator.service.media = {
        record: function(audio, video, ctx) {
            return new StreamRecorder(audio, video, ctx);
        }
    }
}

