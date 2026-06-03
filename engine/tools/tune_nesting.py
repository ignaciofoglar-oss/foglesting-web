#!/usr/bin/env python3
"""Run nesting parameter sweeps and write one Excel workbook with all metrics.

The script intentionally evaluates each parameter set on the full folder and on
deterministic file subsets. That keeps the ranking from overfitting too tightly
to one exact DXF mix.
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import re
import statistics
import subprocess
import sys
import time
import zipfile
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any
from xml.sax.saxutils import escape


DEFAULT_INPUT = Path(os.environ.get("FOGLESTING_TUNE_DXF_DIR", "samples/dxf"))


@dataclass(frozen=True)
class Scenario:
    name: str
    inputs: list[Path]


@dataclass(frozen=True)
class TuneConfig:
    name: str
    optimization_type: str
    optimization_ratio: float
    ga_population: int
    ga_mutation_rate: float
    ga_random_shuffle_prob: float
    ga_random_shuffle_intensity: float
    spacing: float
    rotations: str


def column_name(index: int) -> str:
    letters = ""
    index += 1
    while index:
        index, remainder = divmod(index - 1, 26)
        letters = chr(65 + remainder) + letters
    return letters


def sheet_name(raw: str, used: set[str]) -> str:
    invalid = set('[]:*?/\\')
    clean = "".join(ch if ch not in invalid else "_" for ch in raw).strip()
    clean = clean or "Sheet"
    clean = clean[:31]
    base = clean
    suffix = 2
    while clean in used:
        marker = f"_{suffix}"
        clean = (base[: 31 - len(marker)] + marker)[:31]
        suffix += 1
    used.add(clean)
    return clean


def cell_xml(row_index: int, col_index: int, value: Any) -> str:
    ref = f"{column_name(col_index)}{row_index + 1}"
    if value is None:
        return f'<c r="{ref}"/>'
    if isinstance(value, bool):
        return f'<c r="{ref}" t="b"><v>{1 if value else 0}</v></c>'
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if isinstance(value, float) and (math.isnan(value) or math.isinf(value)):
            value = ""
        else:
            return f'<c r="{ref}"><v>{value}</v></c>'
    text = escape(str(value))
    return f'<c r="{ref}" t="inlineStr"><is><t>{text}</t></is></c>'


def worksheet_xml(rows: list[list[Any]]) -> str:
    row_xml = []
    for r, row in enumerate(rows):
        cells = "".join(cell_xml(r, c, value) for c, value in enumerate(row))
        row_xml.append(f'<row r="{r + 1}">{cells}</row>')
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
        '<sheetViews><sheetView workbookViewId="0"/></sheetViews>'
        f'<sheetData>{"".join(row_xml)}</sheetData>'
        '</worksheet>'
    )


def write_xlsx(path: Path, sheets: list[tuple[str, list[list[Any]]]]) -> None:
    used_names: set[str] = set()
    normalized = [(sheet_name(name, used_names), rows) for name, rows in sheets]
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(
            "[Content_Types].xml",
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="xml" ContentType="application/xml"/>'
            '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
            '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
            + "".join(
                f'<Override PartName="/xl/worksheets/sheet{i}.xml" '
                'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
                for i in range(1, len(normalized) + 1)
            )
            + "</Types>",
        )
        zf.writestr(
            "_rels/.rels",
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
            "</Relationships>",
        )
        zf.writestr(
            "xl/workbook.xml",
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
            'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
            "<sheets>"
            + "".join(
                f'<sheet name="{escape(name)}" sheetId="{i}" r:id="rId{i}"/>'
                for i, (name, _) in enumerate(normalized, start=1)
            )
            + "</sheets></workbook>",
        )
        zf.writestr(
            "xl/_rels/workbook.xml.rels",
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            + "".join(
                f'<Relationship Id="rId{i}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet{i}.xml"/>'
                for i in range(1, len(normalized) + 1)
            )
            + f'<Relationship Id="rId{len(normalized) + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>'
            + "</Relationships>",
        )
        zf.writestr(
            "xl/styles.xml",
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
            '<fonts count="1"><font><sz val="11"/><name val="Calibri"/></font></fonts>'
            '<fills count="1"><fill><patternFill patternType="none"/></fill></fills>'
            '<borders count="1"><border/></borders>'
            '<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>'
            '<cellXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/></cellXfs>'
            "</styleSheet>",
        )
        for i, (_, rows) in enumerate(normalized, start=1):
            zf.writestr(f"xl/worksheets/sheet{i}.xml", worksheet_xml(rows))


def append_config_guide_to_workbook(source: Path, output: Path) -> Path:
    with zipfile.ZipFile(source, "r") as zin:
        names = zin.namelist()
        worksheet_indices = []
        for name in names:
            match = re.fullmatch(r"xl/worksheets/sheet(\d+)\.xml", name)
            if match:
                worksheet_indices.append(int(match.group(1)))
        next_sheet = max(worksheet_indices) + 1

        workbook_xml = zin.read("xl/workbook.xml").decode("utf-8")
        rels_xml = zin.read("xl/_rels/workbook.xml.rels").decode("utf-8")
        types_xml = zin.read("[Content_Types].xml").decode("utf-8")
        next_rid = max(int(value) for value in re.findall(r'Id="rId(\d+)"', rels_xml)) + 1

        guide_sheet = f'<sheet name="Config Guide" sheetId="{next_sheet}" r:id="rId{next_rid}"/>'
        workbook_xml = workbook_xml.replace("</sheets>", guide_sheet + "</sheets>")

        guide_rel = (
            f'<Relationship Id="rId{next_rid}" '
            'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" '
            f'Target="worksheets/sheet{next_sheet}.xml"/>'
        )
        rels_xml = rels_xml.replace("</Relationships>", guide_rel + "</Relationships>")

        guide_type = (
            f'<Override PartName="/xl/worksheets/sheet{next_sheet}.xml" '
            'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
        )
        types_xml = types_xml.replace("</Types>", guide_type + "</Types>")

        guide_xml = worksheet_xml(config_dictionary_rows(candidate_configs()))
        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as zout:
            for name in names:
                if name == "xl/workbook.xml":
                    zout.writestr(name, workbook_xml)
                elif name == "xl/_rels/workbook.xml.rels":
                    zout.writestr(name, rels_xml)
                elif name == "[Content_Types].xml":
                    zout.writestr(name, types_xml)
                else:
                    zout.writestr(name, zin.read(name))
            zout.writestr(f"xl/worksheets/sheet{next_sheet}.xml", guide_xml)
    return output


def candidate_configs() -> list[TuneConfig]:
    return [
        TuneConfig("baseline_bb", "bounding-box", 0.50, 4, 10, 0, 25, 5, "0,90,180,270"),
        TuneConfig("bb_low_width_bias", "bounding-box", 0.35, 6, 12, 10, 25, 5, "0,90,180,270"),
        TuneConfig("bb_high_width_bias", "bounding-box", 0.65, 6, 12, 10, 25, 5, "0,90,180,270"),
        TuneConfig("compact_balanced", "compact-area", 0.50, 6, 12, 10, 25, 5, "0,90,180,270"),
        TuneConfig("compact_low_bias", "compact-area", 0.35, 8, 16, 15, 30, 5, "0,90,180,270"),
        TuneConfig("compact_high_bias", "compact-area", 0.65, 8, 16, 15, 30, 5, "0,90,180,270"),
        TuneConfig("bb_exploratory", "bounding-box", 0.50, 8, 22, 20, 35, 5, "0,90,180,270"),
        TuneConfig("compact_exploratory", "compact-area", 0.50, 8, 22, 20, 35, 5, "0,90,180,270"),
        TuneConfig("bb_shuffle_light", "bounding-box", 0.50, 6, 8, 25, 20, 5, "0,90,180,270"),
        TuneConfig("compact_shuffle_light", "compact-area", 0.50, 6, 8, 25, 20, 5, "0,90,180,270"),
    ]


def broad_candidate_configs() -> list[TuneConfig]:
    configs: list[TuneConfig] = []
    optimization_types = [("bb", "bounding-box"), ("ca", "compact-area")]
    ratios = [0.35, 0.50, 0.65]
    profiles = [
        ("fast", 2, 4, 0, 15),
        ("stable", 4, 8, 0, 20),
        ("local", 4, 8, 10, 15),
        ("balanced", 6, 12, 10, 25),
        ("explore", 6, 18, 20, 30),
        ("strong", 8, 22, 25, 35),
    ]
    for prefix, optimization_type in optimization_types:
        for ratio in ratios:
            ratio_token = str(int(round(ratio * 100)))
            for profile, population, mutation, shuffle_prob, shuffle_intensity in profiles:
                configs.append(TuneConfig(
                    f"{prefix}_r{ratio_token}_{profile}",
                    optimization_type,
                    ratio,
                    population,
                    mutation,
                    shuffle_prob,
                    shuffle_intensity,
                    5,
                    "0,90,180,270",
                ))

    for prefix, optimization_type in optimization_types:
        for profile, population, mutation, shuffle_prob, shuffle_intensity in profiles[:3]:
            configs.append(TuneConfig(
                f"{prefix}_r50_{profile}_rot2",
                optimization_type,
                0.50,
                population,
                mutation,
                shuffle_prob,
                shuffle_intensity,
                5,
                "0,90",
            ))
    return configs


def configs_for_suite(suite: str) -> list[TuneConfig]:
    if suite == "base":
        return candidate_configs()
    if suite == "broad":
        return broad_candidate_configs()
    raise ValueError(f"Unknown suite: {suite}")


CONFIG_DESCRIPTIONS = {
    "baseline_bb": "Configuracion base. Usa Bounding Box, poca poblacion y sin shuffle; sirve como punto de comparacion estable.",
    "bb_low_width_bias": "Bounding Box con ratio 0.35. Le da mas peso a mantener baja la altura del acomodo; busqueda moderada.",
    "bb_high_width_bias": "Bounding Box con ratio 0.65. Le da mas peso a mantener chico el ancho ocupado; busqueda moderada.",
    "compact_balanced": "Area compacta balanceada. Prioriza una forma general mas compacta antes que solo achicar la caja envolvente.",
    "compact_low_bias": "Area compacta con sesgo bajo. Mas poblacion, mas mutacion y shuffle medio; busca alternativas mas agresivas sin romper demasiado el orden.",
    "compact_high_bias": "Area compacta con sesgo alto. Igual que compact_low_bias, pero favorece otro balance ancho/alto.",
    "bb_exploratory": "Bounding Box exploratorio. Mas poblacion, mas mutacion y shuffle local mas fuerte; pensado para escapar de soluciones repetidas.",
    "compact_exploratory": "Area compacta exploratoria. Version mas agresiva de compact-area para comparar contra Bounding Box exploratorio.",
    "bb_shuffle_light": "Bounding Box con mutacion baja pero shuffle local frecuente. Prueba si conviene mover ordenes sin cambiar tanto cada pieza.",
    "compact_shuffle_light": "Area compacta con mutacion baja y shuffle local frecuente. Variante suave para layouts mas compactos.",
}


def config_description(config: TuneConfig) -> str:
    known = CONFIG_DESCRIPTIONS.get(config.name)
    if known:
        return known
    mode = "Bounding Box" if config.optimization_type == "bounding-box" else "Area compacta"
    rotations = "rotaciones completas" if config.rotations == "0,90,180,270" else f"rotaciones {config.rotations}"
    shuffle = (
        "sin shuffle"
        if config.ga_random_shuffle_prob <= 0
        else f"shuffle {config.ga_random_shuffle_prob:g}% intensidad {config.ga_random_shuffle_intensity:g}"
    )
    return (
        f"{mode}, ratio {config.optimization_ratio:g}, poblacion {config.ga_population}, "
        f"mutacion {config.ga_mutation_rate:g}%, {shuffle}, {rotations}."
    )


def config_dictionary_rows(configs: list[TuneConfig]) -> list[list[Any]]:
    rows: list[list[Any]] = [
        ["Hoja", "Que significa"],
        ["Summary", "Todas las corridas en orden de ejecucion."],
        ["Ranked", "Las mismas corridas ordenadas por score: menos chapas primero, despues menor area ocupada y menor tiempo."],
        ["T###...", "Una hoja por test. Arriba estan parametros/comando; abajo estan todas las metricas por iteracion."],
        [],
        ["Campo", "Descripcion"],
        ["Input count", "Cantidad de rutas pasadas al programa. En full_all_files es 1 porque se pasa la carpeta completa; en validation_fold es cantidad de DXF individuales."],
        ["optimization_type", "bounding-box minimiza caja envolvente; compact-area usa un criterio mas balanceado de compactacion."],
        ["optimization_ratio", "0 favorece altura baja; 1 favorece ancho bajo; 0.5 balancea ambos."],
        ["ga_population", "Cantidad de candidatos probados por iteracion. Mas alto explora mas pero tarda mas."],
        ["ga_mutation_rate", "Porcentaje de cambios locales en el orden de piezas."],
        ["ga_random_shuffle_prob", "Probabilidad de aplicar shuffle local a una iteracion/candidato."],
        ["ga_random_shuffle_intensity", "Fuerza del shuffle local. No mezcla todo; mueve piezas dentro de una ventana cercana."],
        ["spacing", "Separacion entre piezas en mm."],
        ["rotations", "Angulos permitidos para rotar piezas."],
        ["best_iteration", "Primera iteracion donde se encontro el resultado final de ese test."],
        ["convergence_pct", "Cuanto bajo el area mejor respecto de la primera solucion."],
        ["time_to_best_seconds", "Segundos transcurridos hasta la primera iteracion donde aparecio la mejor solucion final."],
        ["score", "Ranking balanceado: castiga primero piezas sin ubicar y chapas; despues area, tiempo promedio e iteracion donde encontro la mejor."],
        [],
        ["Configuracion", "optimization_type", "optimization_ratio", "ga_population", "ga_mutation_rate", "ga_random_shuffle_prob", "ga_random_shuffle_intensity", "spacing", "rotations", "Descripcion"],
    ]
    for config in configs:
        rows.append([
            config.name,
            config.optimization_type,
            config.optimization_ratio,
            config.ga_population,
            config.ga_mutation_rate,
            config.ga_random_shuffle_prob,
            config.ga_random_shuffle_intensity,
            config.spacing,
            config.rotations,
            config_description(config),
        ])
    return rows


def discover_dxf_files(input_dir: Path) -> list[Path]:
    files = sorted(input_dir.glob("*.dxf")) + sorted(input_dir.glob("*.DXF"))
    unique: dict[str, Path] = {}
    for file in files:
        unique[str(file).lower()] = file
    return sorted(unique.values(), key=lambda p: p.name.lower())


def build_scenarios(input_dir: Path, include_folds: bool) -> list[Scenario]:
    files = discover_dxf_files(input_dir)
    if not files:
        raise RuntimeError(f"No DXF files found in {input_dir}")
    scenarios = [Scenario("full_all_files", [input_dir])]
    if include_folds and len(files) >= 6:
        by_size = sorted(files, key=lambda p: p.stat().st_size, reverse=True)
        folds = [[], [], []]
        for index, file in enumerate(by_size):
            folds[index % 3].append(file)
        for index, fold in enumerate(folds, start=1):
            scenarios.append(Scenario(f"validation_fold_{index}", sorted(fold, key=lambda p: p.name.lower())))
    return scenarios


def run_command(command: list[str], cwd: Path, timeout: int | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        capture_output=True,
        timeout=timeout,
    )


def read_metrics(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        return []
    rows: list[dict[str, Any]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for raw in csv.DictReader(handle):
            row: dict[str, Any] = {}
            for key, value in raw.items():
                if key == "iteration":
                    row[key] = int(value)
                elif key == "improved":
                    row[key] = int(value)
                else:
                    try:
                        row[key] = float(value)
                    except ValueError:
                        row[key] = value
            rows.append(row)
    return rows


def read_csv_plain(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def summarize_metrics(rows: list[dict[str, Any]], return_code: int, total_seconds: float) -> dict[str, Any]:
    if not rows:
        return {
            "return_code": return_code,
            "total_seconds": total_seconds,
            "iterations_completed": 0,
            "best_iteration": "",
            "final_best_sheet_count": "",
            "final_best_occupied_area": "",
            "final_saved_area": "",
            "final_utilization_pct": "",
            "avg_iteration_seconds": "",
            "median_iteration_seconds": "",
            "p95_iteration_seconds": "",
            "improvement_count": "",
            "convergence_pct": "",
            "time_to_best_seconds": "",
            "score": 1e18,
        }

    last = rows[-1]
    deltas = [float(row["delta_seconds"]) for row in rows if float(row["delta_seconds"]) >= 0.0]
    p95 = sorted(deltas)[min(len(deltas) - 1, max(0, int(math.ceil(len(deltas) * 0.95)) - 1))] if deltas else ""
    final_best_area = float(last["best_occupied_area"])
    final_sheets = int(last["best_sheet_count"])
    final_unplaced = int(last["best_unplaced"])
    baseline_area = max(1.0, float(rows[0]["best_occupied_area"]))
    convergence_pct = max(0.0, (baseline_area - final_best_area) / baseline_area * 100.0)

    best_iteration = int(last["iteration"])
    for row in rows:
        if (
            int(row["best_sheet_count"]) == final_sheets
            and int(row["best_unplaced"]) == final_unplaced
            and abs(float(row["best_occupied_area"]) - final_best_area) <= 1e-6
        ):
            best_iteration = int(row["iteration"])
            break
    time_to_best_seconds = float(rows[best_iteration - 1]["elapsed_seconds"]) if best_iteration - 1 < len(rows) else total_seconds

    avg_delta = statistics.mean(deltas) if deltas else 0.0
    median_delta = statistics.median(deltas) if deltas else 0.0

    score = (
        final_unplaced * 1e15
        + final_sheets * 1e12
        + final_best_area
        + avg_delta * 100000.0
        + best_iteration * 50000.0
        + time_to_best_seconds * 25000.0
    )

    return {
        "return_code": return_code,
        "total_seconds": total_seconds,
        "iterations_completed": int(last["iteration"]),
        "best_iteration": best_iteration,
        "final_best_sheet_count": final_sheets,
        "final_best_occupied_area": final_best_area,
        "final_saved_area": float(last["saved_area"]),
        "final_utilization_pct": float(last["best_utilization"]) * 100.0,
        "avg_iteration_seconds": avg_delta if deltas else "",
        "median_iteration_seconds": median_delta if deltas else "",
        "p95_iteration_seconds": p95,
        "improvement_count": sum(int(row["improved"]) for row in rows),
        "convergence_pct": convergence_pct,
        "time_to_best_seconds": time_to_best_seconds,
        "score": score,
    }


def metadata_rows(
    run_id: int,
    scenario: Scenario,
    config: TuneConfig,
    summary: dict[str, Any],
    command: list[str],
    stdout: str,
    stderr: str,
) -> list[list[Any]]:
    return [
        ["Run ID", run_id],
        ["Scenario", scenario.name],
        ["Input count", len(scenario.inputs)],
        ["Config", config.name],
        ["Optimization type", config.optimization_type],
        ["Optimization ratio", config.optimization_ratio],
        ["GA population", config.ga_population],
        ["GA mutation rate", config.ga_mutation_rate],
        ["GA random shuffle prob", config.ga_random_shuffle_prob],
        ["GA random shuffle intensity", config.ga_random_shuffle_intensity],
        ["Spacing", config.spacing],
        ["Rotations", config.rotations],
        ["Return code", summary["return_code"]],
        ["Total seconds", summary["total_seconds"]],
        ["Iterations completed", summary["iterations_completed"]],
        ["Best iteration", summary["best_iteration"]],
        ["Final best sheets", summary["final_best_sheet_count"]],
        ["Final best occupied area", summary["final_best_occupied_area"]],
        ["Final saved area", summary["final_saved_area"]],
        ["Final utilization pct", summary["final_utilization_pct"]],
        ["Avg iteration seconds", summary["avg_iteration_seconds"]],
        ["Median iteration seconds", summary["median_iteration_seconds"]],
        ["P95 iteration seconds", summary["p95_iteration_seconds"]],
        ["Improvement count", summary["improvement_count"]],
        ["Convergence pct", summary["convergence_pct"]],
        ["Time to best seconds", summary["time_to_best_seconds"]],
        ["Score", summary["score"]],
        ["Command", " ".join(command)],
        ["Stdout", stdout[-30000:]],
        ["Stderr", stderr[-30000:]],
        [],
        ["Inputs"],
        *[[str(item)] for item in scenario.inputs],
        [],
    ]


def rebuild_existing_workbook(run_dir: Path, output_path: Path | None = None) -> Path:
    configs = candidate_configs()
    config_by_name = {config.name: config for config in configs}
    summary_path = run_dir / "nesting_tuning_results.xlsx"
    workbook_path = output_path or (run_dir / "nesting_tuning_results_with_config_guide.xlsx")

    scenarios = ["full_all_files", "validation_fold_1", "validation_fold_2", "validation_fold_3"]
    metric_files = sorted(run_dir.glob("*_metrics.csv"))
    if not metric_files:
        raise RuntimeError(f"No metrics CSV files found in {run_dir}")

    summary_rows: list[list[Any]] = [[
        "run_id",
        "scenario",
        "config",
        "return_code",
        "total_seconds",
        "iterations_completed",
        "best_iteration",
        "final_best_sheet_count",
        "final_best_occupied_area",
        "final_saved_area",
        "final_utilization_pct",
        "avg_iteration_seconds",
        "median_iteration_seconds",
        "p95_iteration_seconds",
        "improvement_count",
        "convergence_pct",
        "time_to_best_seconds",
        "score",
        "output_dxf",
        "metrics_csv",
    ]]
    all_sheets: list[tuple[str, list[list[Any]]]] = []

    for metrics_path in metric_files:
        stem = metrics_path.name.removesuffix("_metrics.csv")
        run_token, rest = stem.split("_", 1)
        scenario = next((item for item in scenarios if rest.startswith(item + "_")), "")
        if not scenario:
            continue
        config_name = rest[len(scenario) + 1 :]
        config = config_by_name.get(config_name)
        metrics = read_metrics(metrics_path)
        summary = summarize_metrics(metrics, 0, float(metrics[-1]["elapsed_seconds"]) if metrics else 0.0)
        dxf_path = metrics_path.with_name(stem + ".dxf")
        run_id = int(run_token.removeprefix("T"))
        summary_rows.append([
            run_id,
            scenario,
            config_name,
            summary["return_code"],
            summary["total_seconds"],
            summary["iterations_completed"],
            summary["best_iteration"],
            summary["final_best_sheet_count"],
            summary["final_best_occupied_area"],
            summary["final_saved_area"],
            summary["final_utilization_pct"],
            summary["avg_iteration_seconds"],
            summary["median_iteration_seconds"],
            summary["p95_iteration_seconds"],
            summary["improvement_count"],
            summary["convergence_pct"],
            summary["time_to_best_seconds"],
            summary["score"],
            str(dxf_path),
            str(metrics_path),
        ])

        rows: list[list[Any]] = [
            ["Run ID", run_id],
            ["Scenario", scenario],
            ["Config", config_name],
        ]
        if config:
            rows.extend([
                ["Descripcion", config_description(config)],
                ["Optimization type", config.optimization_type],
                ["Optimization ratio", config.optimization_ratio],
                ["GA population", config.ga_population],
                ["GA mutation rate", config.ga_mutation_rate],
                ["GA random shuffle prob", config.ga_random_shuffle_prob],
                ["GA random shuffle intensity", config.ga_random_shuffle_intensity],
                ["Spacing", config.spacing],
                ["Rotations", config.rotations],
            ])
        rows.extend([
            ["Best iteration", summary["best_iteration"]],
            ["Final best sheets", summary["final_best_sheet_count"]],
            ["Final best occupied area", summary["final_best_occupied_area"]],
            ["Final utilization pct", summary["final_utilization_pct"]],
            ["Time to best seconds", summary["time_to_best_seconds"]],
            [],
            ["Iteration Metrics"],
        ])
        headers = list(metrics[0].keys()) if metrics else []
        rows.append(headers)
        for metric in metrics:
            rows.append([metric.get(header, "") for header in headers])
        all_sheets.append((stem, rows))

    ranked = sorted(summary_rows[1:], key=lambda row: (row[3] != 0, row[17]))
    all_sheets.insert(0, ("Summary", summary_rows))
    all_sheets.insert(1, ("Ranked", [summary_rows[0], *ranked]))
    all_sheets.insert(2, ("Config Guide", config_dictionary_rows(configs)))
    write_xlsx(workbook_path, all_sheets)
    if summary_path.exists():
        print(f"Original workbook kept: {summary_path}")
    print(f"Workbook with guide written: {workbook_path}")
    return workbook_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Tune Sheet Metal Nesting parameters and write an Excel workbook.")
    parser.add_argument("--input", default=str(DEFAULT_INPUT), help="DXF folder to tune against.")
    parser.add_argument("--iterations", type=int, default=20, help="Iterations per run.")
    parser.add_argument("--sheet-width", type=float, default=3000.0)
    parser.add_argument("--sheet-height", type=float, default=1500.0)
    parser.add_argument("--cpu-cores", type=int, default=max(1, min(8, os.cpu_count() or 4)))
    parser.add_argument("--solver", default="cpu", choices=["cpu", "gpu", "dual-gpu"])
    parser.add_argument("--suite", default="base", choices=["base", "broad"], help="Parameter set to test.")
    parser.add_argument("--config-filter", default="", help="Comma-separated config names to run from the selected suite.")
    parser.add_argument("--max-runs", type=int, default=0, help="0 means run every config/scenario combination.")
    parser.add_argument("--no-folds", action="store_true", help="Only test the full folder.")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--timeout", type=int, default=1800, help="Timeout per run in seconds.")
    parser.add_argument("--output", default="", help="Output .xlsx path.")
    parser.add_argument("--rebuild-from", default="", help="Existing tuning output folder. Rebuild workbook without rerunning tests.")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    if args.rebuild_from:
        run_dir = Path(args.rebuild_from)
        original = run_dir / "nesting_tuning_results.xlsx"
        output = Path(args.output) if args.output else run_dir / "nesting_tuning_results_original_values_with_config_guide.xlsx"
        if original.exists():
            append_config_guide_to_workbook(original, output)
            print(f"Original workbook kept: {original}")
            print(f"Workbook with original values and guide written: {output}")
        else:
            rebuild_existing_workbook(run_dir, output)
        return 0

    exe = root / "build" / "sheet_nest_cli.exe"
    output_dir = root / "outputs" / "tuning" / datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir.mkdir(parents=True, exist_ok=True)
    workbook_path = Path(args.output) if args.output else output_dir / "nesting_tuning_results.xlsx"

    if not args.skip_build:
        print("Building C++ project...")
        build = run_command(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "build.ps1"], root, timeout=600)
        if build.returncode != 0:
            print(build.stdout)
            print(build.stderr, file=sys.stderr)
            return build.returncode

    if not exe.exists():
        raise RuntimeError(f"CLI executable not found: {exe}")

    scenarios = build_scenarios(Path(args.input), include_folds=not args.no_folds)
    configs = configs_for_suite(args.suite)
    if args.config_filter:
        wanted = {item.strip() for item in args.config_filter.split(",") if item.strip()}
        configs = [config for config in configs if config.name in wanted]
        missing = sorted(wanted - {config.name for config in configs})
        if missing:
            raise RuntimeError(f"Config names not found in suite {args.suite}: {', '.join(missing)}")
    planned = [(config, scenario) for config in configs for scenario in scenarios]
    if args.max_runs > 0:
        planned = planned[: args.max_runs]

    all_sheets: list[tuple[str, list[list[Any]]]] = []
    summary_rows: list[list[Any]] = [[
        "run_id",
        "scenario",
        "config",
        "return_code",
        "total_seconds",
        "iterations_completed",
        "best_iteration",
        "final_best_sheet_count",
        "final_best_occupied_area",
        "final_saved_area",
        "final_utilization_pct",
        "avg_iteration_seconds",
        "median_iteration_seconds",
        "p95_iteration_seconds",
        "improvement_count",
        "convergence_pct",
        "time_to_best_seconds",
        "score",
        "output_dxf",
        "metrics_csv",
    ]]

    metric_headers: list[str] = []
    for run_index, (config, scenario) in enumerate(planned, start=1):
        run_name = f"T{run_index:03d}_{scenario.name}_{config.name}"
        metrics_path = output_dir / f"{run_name}_metrics.csv"
        dxf_path = output_dir / f"{run_name}.dxf"
        command = [
            str(exe),
            "--sheet-width", str(args.sheet_width),
            "--sheet-height", str(args.sheet_height),
            "--spacing", str(config.spacing),
            "--rotations", config.rotations,
            "--iterations", str(args.iterations),
            "--cpu-cores", str(args.cpu_cores),
            "--ga-population", str(config.ga_population),
            "--ga-mutation-rate", str(config.ga_mutation_rate),
            "--ga-random-shuffle-prob", str(config.ga_random_shuffle_prob),
            "--ga-random-shuffle-intensity", str(config.ga_random_shuffle_intensity),
            "--optimization-type", config.optimization_type,
            "--optimization-ratio", str(config.optimization_ratio),
            "--solver", args.solver,
            "--metrics-csv", str(metrics_path),
            "--output", str(dxf_path),
        ]
        for input_path in scenario.inputs:
            command.extend(["--input", str(input_path)])

        print(f"[{run_index}/{len(planned)}] {scenario.name} / {config.name}")
        started = time.perf_counter()
        try:
            proc = run_command(command, root, timeout=args.timeout)
            stdout = proc.stdout
            stderr = proc.stderr
            return_code = proc.returncode
        except subprocess.TimeoutExpired as error:
            stdout = error.stdout or ""
            stderr = (error.stderr or "") + f"\nTIMEOUT after {args.timeout} seconds"
            return_code = 124
        total_seconds = time.perf_counter() - started

        metrics = read_metrics(metrics_path)
        if metrics and not metric_headers:
            metric_headers = list(metrics[0].keys())
        summary = summarize_metrics(metrics, return_code, total_seconds)

        summary_rows.append([
            run_index,
            scenario.name,
            config.name,
            summary["return_code"],
            summary["total_seconds"],
            summary["iterations_completed"],
            summary["best_iteration"],
            summary["final_best_sheet_count"],
            summary["final_best_occupied_area"],
            summary["final_saved_area"],
            summary["final_utilization_pct"],
            summary["avg_iteration_seconds"],
            summary["median_iteration_seconds"],
            summary["p95_iteration_seconds"],
            summary["improvement_count"],
            summary["convergence_pct"],
            summary["time_to_best_seconds"],
            summary["score"],
            str(dxf_path),
            str(metrics_path),
        ])

        rows = metadata_rows(run_index, scenario, config, summary, command, stdout, stderr)
        rows.append(["Iteration Metrics"])
        headers = metric_headers or (list(metrics[0].keys()) if metrics else [])
        rows.append(headers)
        for metric in metrics:
            rows.append([metric.get(header, "") for header in headers])
        all_sheets.append((run_name, rows))

    ranked = sorted(summary_rows[1:], key=lambda row: (row[3] != 0, row[17]))
    ranked_rows = [summary_rows[0], *ranked]
    all_sheets.insert(0, ("Summary", summary_rows))
    all_sheets.insert(1, ("Ranked", ranked_rows))
    all_sheets.insert(2, ("Config Guide", config_dictionary_rows(configs)))
    write_xlsx(workbook_path, all_sheets)

    print(f"Excel written: {workbook_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
