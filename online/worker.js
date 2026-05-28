// ====================================================
// Foglesting Online — WASM Web Worker
// Runs nesting in background, yields previews
// ====================================================

importScripts('nesting.js?v=fast-init-v1');

let nestingModule = null;

createNestingModule().then(Module => {
    nestingModule = Module;
    postMessage({ type: 'ready' });
}).catch(err => {
    postMessage({ type: 'error', data: err.message });
});

onmessage = function(e) {
    if (e.data.type === 'run') {
        try {
            const { files, spacing, iterations, population, rotations, sheetWidth, sheetHeight } = e.data;
            
            let quantities = [];
            for (let i = 0; i < files.length; i++) {
                const qty = files[i].quantity || 1;
                quantities.push(qty);
                nestingModule.FS.writeFile(`input_${i}.dxf`, files[i].buffer);
            }
            const quantities_str = quantities.join(',');

            let last_post_time = 0;
            const progress_callback = function(json_str) {
                const now = Date.now();
                if (now - last_post_time > 50) { // Update UI at most ~20 times per second
                    postMessage({ type: 'preview', data: json_str });
                    last_post_time = now;
                }
                return true; 
            };

            const result_json = nestingModule.run_nesting_wasm(
                quantities_str, sheetWidth, sheetHeight, population, iterations, 10.0, 10.0, spacing, rotations,
                progress_callback
            );

            let output_dxf_buffer = null;
            try {
                if (nestingModule.FS.analyzePath('output.dxf').exists) {
                    output_dxf_buffer = nestingModule.FS.readFile('output.dxf', { encoding: 'binary' });
                }
            } catch (ex) {}

            postMessage({ 
                type: 'done', 
                data: result_json,
                dxf_buffer: output_dxf_buffer 
            });

        } catch (err) {
            postMessage({ type: 'error', data: err.toString() });
        }
    }
};
