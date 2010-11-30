/* 
 * Wrap IMediaRecorder in HTML5 device like API. Not quite there, but close.
 * http://www.whatwg.org/specs/web-apps/current-work/complete/commands.html#devices
 */
function StreamFRecorder(params, ctx)
{
    recFStart(params, ctx);
    return this;    
}
StreamFRecorder.prototype.stop = function()
{
    return recStop();
}

function StreamTRecorder(params, ctx, host, port)
{
    recTStart(params, ctx, host, port);
    return this;
}
StreamTRecorder.prototype.stop = function()
{
    return recStop();
}

function SocketRecorder(params, ctx, sock)
{
    recSStart(params, ctx, sock);
    return this;
}
SocketRecorder.prototype.stop = function()
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
        recordToFile: function(params, ctx) {
            return new StreamFRecorder(params, ctx);
        },
        recordToStream: function(params, ctx, host, port) {
            return new StreamTRecorder(params, ctx, host, port);
        },
        recordToSocket: function(params, ctx, sock) {
            return new SocketRecorder(params, ctx, sock);
        }
    }
}

