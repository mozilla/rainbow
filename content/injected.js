/* Expose API under window.navigator.service.media */
if (window && window.navigator) {
    if (!window.navigator.service)
        window.navigator.service = {};
    window.navigator.service.media = {
        recordToFile: function(params, ctx, obs) {
            return recStart(window.location, params, ctx, obs);
        },
        stop: function() {
            return recStop(window.location);
        }
    }
}

