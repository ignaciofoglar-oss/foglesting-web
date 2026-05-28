// ====================================================
// Foglesting Online — WASM Web Worker
// Runs nesting in background, yields previews
// ====================================================

importScripts('nesting.js?v=solver-options-info-20260528');

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
            const { files, spacing, iterations, population, rotations, sheetWidth, sheetHeight, optimizationType } = e.data;

            try {
                if (nestingModule.FS.analyzePath('output.dxf').exists) {
                    nestingModule.FS.unlink('output.dxf');
                }
            } catch (ex) {}
            
            let quantities = [];
            for (let i = 0; i < files.length; i++) {
                const qty = files[i].quantity || 1;
                quantities.push(qty);
                try {
                    if (nestingModule.FS.analyzePath(`input_${i}.dxf`).exists) {
                        nestingModule.FS.unlink(`input_${i}.dxf`);
                    }
                } catch (ex) {}
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
                optimizationType || 'compact-area',
                progress_callback
            );

            let output_dxf_buffer = null;
            try {
                if (nestingModule.FS.analyzePath('output.dxf').exists) {
                    const dxf_data = nestingModule.FS.readFile('output.dxf');
                    output_dxf_buffer = new Uint8Array(dxf_data);
                }
            } catch (ex) {}

            postMessage({ 
                type: 'done', 
                data: result_json,
                dxf_buffer: output_dxf_buffer 
            }, output_dxf_buffer ? [output_dxf_buffer.buffer] : []);

        } catch (err) {
            postMessage({ type: 'error', data: err.toString() });
        }
    }
};
