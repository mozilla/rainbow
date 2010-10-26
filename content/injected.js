/* 
 * Wrap IMediaRecorder in HTML5 device like API. Not quite there, but close.
 * http://www.whatwg.org/specs/web-apps/current-work/complete/commands.html#devices
 */
function StreamRecorder(audio, video, canvas)
{
    this._file = recStart(audio, video, canvas);
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

