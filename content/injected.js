/* 
 * Wrap IMediaRecorder in HTML5 device like API. Not quite there, but close.
 * http://www.whatwg.org/specs/web-apps/current-work/complete/commands.html#devices
 */
function StreamRecorder(params, ctx, obs)
{
    recStart(params, ctx, obs);
    return this;    
}
StreamRecorder.prototype.stop = function()
{
    return recStop();
}

/*
 * Expose media API just like the Mozilla Labs: Contacts addon
 */
if (window && window.navigator) {
    if (!window.navigator.service)
        window.navigator.service = {};
    window.navigator.service.media = {
        recordToFile: function(params, ctx, obs) {
            return new StreamRecorder(params, ctx, obs);
        }
    }
}

