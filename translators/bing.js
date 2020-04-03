var lastText = '';
var active = window.location.href !== "about:blank";

function checkFinished() {
    if (!active) return;

    let area = document.querySelector('#tta_output_ta')
    if (!area)
        return;

    let text = area.value.trim();
    if (text === lastText || text === lastText + ' ...' || text === '' || text === '...')
        return;

    console.log('translated text', text, 'old', lastText, 'size', text.length, lastText.length);
    lastText = text;
    active = false;
    proxy.setTranslated(text);
}

function translate(text, from, to) {
    console.log('start translate', text, from, to)
    active = true;

    if (window.location.href.indexOf('bing.com/translator') !== -1
        && window.location.href.indexOf('&to=' + to + '&') !== -1) {
        document.querySelector('textarea#tta_input_ta').value = text;
        document.querySelector('textarea#tta_input_ta').dispatchEvent(
            new Event("input", { bubbles: true, cancelable: true }));
        return;
    }

    let url = 'https://www.bing.com/translator/?from=auto&to=' + to + '&text=' + encodeURIComponent(text);
    console.log("setting url", url);
    window.location = url;

}

function init() {
    proxy.translate.connect(translate);
    setInterval(checkFinished, 300);
}
