/* Expose API under window.navigator.service.media */
if (window && window.navigator) {
    if (!window.navigator.service)
        window.navigator.service = {};
    window.navigator.service.media = {
        beginSession: function(params, ctx, audio, slr, obs) {
            return sessStart(window.location, params, ctx, audio, slr, obs);
        },
        beginRecording: function() {
            return recStart(window.location);
        },
        endRecording: function() {
            return recStop(window.location);
        },
        endSession: function() {
            return sessStop(window.location);
        },
        fetchImage: function(isFile) {
            return fetchImg(window.location, isFile);
        }
    }
}

