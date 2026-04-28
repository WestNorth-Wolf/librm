/**
 * DspFilterBodePlot — ArduPilot 滤波器的交互式 Bode 图组件集合
 *
 * 导出：
 *  LowPass1pBodePlot      一阶低通（LowPassFilter / LowPassFilterConstDt）
 *  LowPass2pBodePlot      二阶 Butterworth（LowPassFilter2p）
 *  NotchFilterBodePlot    陷波滤波器（NotchFilter）
 *  HarmonicNotchBodePlot  谐波陷波滤波器（HarmonicNotchFilter）
 *  AverageFilterBodePlot  滑动平均滤波器（AverageFilter）
 *  DerivativeFilterBodePlot 平滑微分滤波器（DerivativeFilter）
 */

import React, { useState, useMemo } from "react";
import {
  Chart as ChartJS,
  LogarithmicScale,
  LinearScale,
  PointElement,
  LineElement,
  Tooltip,
  Legend,
} from "chart.js";
import { Line } from "react-chartjs-2";

ChartJS.register(
  LogarithmicScale,
  LinearScale,
  PointElement,
  LineElement,
  Tooltip,
  Legend,
);

// ─── 常量 ─────────────────────────────────────────────────────────────────────

const N = 300; // 频率采样点数
const H = 240; // 子图高度(px)

const CARD: React.CSSProperties = {
  border: "1px solid var(--ifm-color-emphasis-300)",
  borderRadius: 8,
  padding: "1.25rem",
  marginBottom: "1.5rem",
};
const ROW: React.CSSProperties = {
  display: "flex",
  flexWrap: "wrap",
  gap: "1.25rem",
  marginBottom: "1rem",
  alignItems: "center",
};
const SEC: React.CSSProperties = {
  fontSize: "0.8rem",
  fontWeight: 600,
  marginBottom: 4,
  color: "var(--ifm-color-emphasis-700)",
};
const INPUT_STYLE: React.CSSProperties = {
  width: 110,
  padding: "0.3rem 0.5rem",
  border: "1px solid var(--ifm-color-emphasis-300)",
  borderRadius: 4,
  background: "var(--ifm-background-color)",
  color: "var(--ifm-font-color-base)",
};

// ─── 工具函数 ─────────────────────────────────────────────────────────────────

/** 对数均匀分布频率数组，范围 [max(0.1, fs/40000), fs/2] */
function logFreqs(fs: number): number[] {
  const fMin = Math.max(0.1, fs / 40000);
  const fMax = fs / 2;
  const arr: number[] = [];
  for (let i = 0; i < N; i++)
    arr.push(fMin * Math.pow(fMax / fMin, i / (N - 1)));
  return arr;
}

/** 双二阶滤波器频率响应
 *  H(z) = (b0 + b1·z⁻¹ + b2·z⁻²) / (1 + a1·z⁻¹ + a2·z⁻²)
 */
function biquadResp(
  freqs: number[],
  fs: number,
  b0: number,
  b1: number,
  b2: number,
  a1: number,
  a2: number,
) {
  return freqs.map((f) => {
    const θ = (2 * Math.PI * f) / fs;
    const c1 = Math.cos(θ),
      s1 = Math.sin(θ);
    const c2 = Math.cos(2 * θ),
      s2 = Math.sin(2 * θ);
    const nRe = b0 + b1 * c1 + b2 * c2;
    const nIm = -(b1 * s1 + b2 * s2);
    const dRe = 1 + a1 * c1 + a2 * c2;
    const dIm = -(a1 * s1 + a2 * s2);
    const mag = Math.hypot(nRe, nIm) / Math.max(Math.hypot(dRe, dIm), 1e-12);
    return {
      magDb: 20 * Math.log10(Math.max(mag, 1e-12)),
      phase: (Math.atan2(nIm, nRe) - Math.atan2(dIm, dRe)) * (180 / Math.PI),
    };
  });
}

/** 精确 -3dB 带宽（一阶 IIR）
 *  令 |H|² = 0.5 解析求解 cos(θ_c)
 */
function lp1pBandwidth(alpha: number, fs: number): number {
  if (alpha >= 1) return fs / 2;
  const β = 1 - alpha;
  const cosθ = (1 + β * β - 2 * alpha * alpha) / (2 * β);
  if (cosθ <= -1) return fs / 2;
  if (cosθ >= 1) return 0;
  return (Math.acos(cosθ) * fs) / (2 * Math.PI);
}

/** 数字频率→Hz 显示格式 */
function fmtHz(f: number): string {
  if (f >= 1000) return `${(f / 1000).toFixed(2)} kHz`;
  if (f >= 10) return `${f.toFixed(1)} Hz`;
  return `${f.toFixed(2)} Hz`;
}

// ─── 共享控件 ─────────────────────────────────────────────────────────────────

function Slider({
  label,
  value,
  min,
  max,
  step,
  unit = "",
  desc,
  fmt,
  onChange,
}: {
  label: string;
  value: number;
  min: number;
  max: number;
  step: number;
  unit?: string;
  desc?: string;
  fmt?: (v: number) => string;
  onChange: (v: number) => void;
}) {
  const display = fmt ? fmt(value) : `${value}${unit}`;
  return (
    <label
      style={{ display: "flex", flexDirection: "column", gap: 4, flex: "1 1 200px" }}
    >
      <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>
        {label} ={" "}
        <span style={{ color: "rgb(99,102,241)" }}>{display}</span>
      </span>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => onChange(parseFloat(e.target.value))}
        style={{ width: "100%" }}
      />
      {desc && (
        <span style={{ fontSize: "0.75rem", color: "var(--ifm-color-emphasis-600)" }}>
          {desc}
        </span>
      )}
    </label>
  );
}

/** 对数刻度频率滑块
 *  - 内部线性 [0, 1000] 映射到对数频率，每个十倍程占相同滑块行程
 *  - 输出精度：<10 Hz → 0.01，<100 Hz → 0.1，≥100 Hz → 1
 */
function FreqSlider({
  label,
  value,
  min,
  max,
  desc,
  onChange,
}: {
  label: string;
  value: number;
  min: number;
  max: number;
  desc?: string;
  onChange: (v: number) => void;
}) {
  const safeMin = Math.max(min, 0.001);
  const safeMax = Math.max(max, safeMin + 0.001);

  const toInternal = (f: number) =>
    Math.round(
      (Math.log(Math.max(f, safeMin) / safeMin) / Math.log(safeMax / safeMin)) * 1000,
    );

  const toFreq = (v: number) => {
    const raw = safeMin * Math.pow(safeMax / safeMin, v / 1000);
    const step = raw < 10 ? 0.01 : raw < 100 ? 0.1 : 1;
    return Math.round(raw / step) * step;
  };

  const internalValue = toInternal(Math.max(safeMin, Math.min(safeMax, value)));

  const fmt = (f: number) =>
    f < 10 ? `${f.toFixed(2)} Hz` : f < 100 ? `${f.toFixed(1)} Hz` : `${f.toFixed(0)} Hz`;

  return (
    <label style={{ display: "flex", flexDirection: "column", gap: 4, flex: "1 1 200px" }}>
      <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>
        {label} = <span style={{ color: "rgb(99,102,241)" }}>{fmt(value)}</span>
      </span>
      <input
        type="range"
        min={0}
        max={1000}
        step={1}
        value={internalValue}
        onChange={(e) => {
          const freq = toFreq(parseInt(e.target.value, 10));
          onChange(Math.max(safeMin, Math.min(safeMax, freq)));
        }}
        style={{ width: "100%" }}
      />
      {desc && (
        <span style={{ fontSize: "0.75rem", color: "var(--ifm-color-emphasis-600)" }}>
          {desc}
        </span>
      )}
    </label>
  );
}

function NumInput({
  label,
  value,
  min,
  max,
  step,
  onChange,
}: {
  label: React.ReactNode;
  value: number;
  min: number;
  max: number;
  step: number;
  onChange: (v: number) => void;
}) {
  return (
    <label style={{ display: "flex", flexDirection: "column", gap: 4, flex: "0 0 auto" }}>
      <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>{label}</span>
      <input
        type="number"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => {
          const v = parseFloat(e.target.value);
          if (v >= min && v <= max) onChange(v);
        }}
        style={INPUT_STYLE}
      />
    </label>
  );
}

function Stat({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <div
      style={{ fontSize: "0.85rem", color: "var(--ifm-color-emphasis-700)", flex: "1 1 160px" }}
    >
      {children}
    </div>
  );
}

// ─── 共享 Bode 图渲染器 ───────────────────────────────────────────────────────

type DS = {
  label: string;
  data: number[];
  borderColor: string;
  backgroundColor?: string;
  borderWidth?: number;
  borderDash?: number[];
  fill?: boolean;
  pointRadius?: number;
  tension?: number;
};

interface BodePlotProps {
  labels: string[];
  magDatasets: DS[];
  phaseDatasets: DS[];
  magMin?: number;
  magMax?: number;
  phaseMin?: number;
  phaseMax?: number;
}

function BodePlot({
  labels,
  magDatasets,
  phaseDatasets,
  magMin,
  magMax = 5,
  phaseMin = -200,
  phaseMax = 100,
}: BodePlotProps) {
  const finite = magDatasets[0]?.data.filter(isFinite) ?? [];
  const autoMin =
    finite.length > 0
      ? Math.floor(Math.min(...finite) / 10) * 10 - 5
      : -80;
  const computedMagMin = magMin ?? Math.min(autoMin, -60);

  function opts(yLabel: string, yMin: number, yMax: number, legend: boolean) {
    return {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 0 } as const,
      interaction: { mode: "index" as const, intersect: false },
      plugins: {
        legend: {
          display: legend,
          position: "top" as const,
          labels: { boxWidth: 18, font: { size: 11 } },
        },
        tooltip: {
          callbacks: {
            title: (items: { label: string }[]) => {
              const f = parseFloat(items[0]?.label ?? "0");
              return fmtHz(f);
            },
          },
        },
      },
      scales: {
        x: {
          type: "logarithmic" as const,
          title: { display: true, text: "频率 (Hz)" },
          ticks: {
            maxTicksLimit: 8,
            callback: (value: number | string) => {
              const v = Number(value);
              const anchors = [0.1, 1, 10, 100, 1000, 10000, 50000];
              if (anchors.some((a) => Math.abs(v - a) / a < 0.02))
                return v >= 1000 ? `${v / 1000}k` : `${v}`;
              return null;
            },
          },
        },
        y: { title: { display: true, text: yLabel }, min: yMin, max: yMax },
      },
    };
  }

  return (
    <>
      <div style={{ marginBottom: "0.75rem" }}>
        <div style={SEC}>幅频响应</div>
        <div style={{ height: H }}>
          <Line
            data={{ labels, datasets: magDatasets as Parameters<typeof Line>[0]["data"]["datasets"] }}
            options={opts("幅度 (dB)", computedMagMin, magMax, true)}
          />
        </div>
      </div>
      <div>
        <div style={SEC}>相频响应</div>
        <div style={{ height: H }}>
          <Line
            data={{
              labels,
              datasets: phaseDatasets as Parameters<typeof Line>[0]["data"]["datasets"],
            }}
            options={opts("相位 (°)", phaseMin, phaseMax, phaseDatasets.length > 1)}
          />
        </div>
      </div>
    </>
  );
}

/** 主幅频数据集（蓝色填充曲线） */
function magMainDS(data: number[]): DS {
  return {
    label: "幅度",
    data,
    borderColor: "rgb(99,102,241)",
    backgroundColor: "rgba(99,102,241,0.1)",
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.1,
    fill: true,
  };
}

/** -3dB 参考水平线 */
function neg3dBLine(n: number): DS {
  return {
    label: "-3 dB",
    data: new Array(n).fill(-3),
    borderColor: "rgba(239,68,68,0.85)",
    borderWidth: 1.5,
    borderDash: [6, 4],
    pointRadius: 0,
    fill: false,
    tension: 0,
  };
}

/** 主相频数据集（橙色） */
function phaseMainDS(data: number[]): DS {
  return {
    label: "相位",
    data,
    borderColor: "rgb(234,88,12)",
    backgroundColor: "rgba(234,88,12,0.1)",
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.1,
    fill: true,
  };
}

// ─── 1. 一阶低通滤波器 ────────────────────────────────────────────────────────
// alpha = (2π·fc/fs) / (1 + 2π·fc/fs)
// H(z) = α / (1 − (1−α)z⁻¹)

export function LowPass1pBodePlot() {
  const [fc, setFc] = useState(50);
  const [fs, setFs] = useState(1000);

  const freqs = useMemo(() => logFreqs(fs), [fs]);
  const labels = useMemo(() => freqs.map((f) => f.toFixed(4)), [freqs]);

  const { mags, phases, bw, alpha } = useMemo(() => {
    const fc_c = Math.min(fc, fs / 2 - 0.1);
    const a = (2 * Math.PI * fc_c) / (fs + 2 * Math.PI * fc_c);
    const β = 1 - a;
    const mags: number[] = [];
    const phases: number[] = [];
    for (const f of freqs) {
      const θ = (2 * Math.PI * f) / fs;
      const re = 1 - β * Math.cos(θ);
      const im = β * Math.sin(θ);
      mags.push(20 * Math.log10(a / Math.hypot(re, im)));
      phases.push(-Math.atan2(im, re) * (180 / Math.PI));
    }
    return { mags, phases, bw: lp1pBandwidth(a, fs), alpha: a };
  }, [fc, fs, freqs]);

  return (
    <div style={CARD}>
      <div style={ROW}>
        <FreqSlider
          label="截止频率 fc"
          value={fc}
          min={0.1}
          max={Math.floor(fs / 2 - 1)}
          onChange={setFc}
        />
        <NumInput
          label={<>采样率 f<sub>s</sub> (Hz)</>}
          value={fs}
          min={10}
          max={100000}
          step={100}
          onChange={setFs}
        />
        <Stat>
          α = <strong style={{ color: "rgb(99,102,241)" }}>{alpha.toFixed(4)}</strong>
          <br />
          带宽 (−3 dB){" "}
          <strong style={{ color: "rgb(99,102,241)" }}>{fmtHz(bw)}</strong>
          <br />
          奈奎斯特 {fmtHz(fs / 2)}
        </Stat>
      </div>
      <BodePlot
        labels={labels}
        magDatasets={[magMainDS(mags), neg3dBLine(N)]}
        phaseDatasets={[phaseMainDS(phases)]}
        phaseMin={-100}
        phaseMax={5}
      />
    </div>
  );
}

// ─── 2. 二阶 Butterworth 低通（LowPassFilter2p） ─────────────────────────────
// ohm = tan(π·fc_c/fs)，c = 1 + √2·ohm + ohm²
// b0 = ohm²/c，b1 = 2b0，b2 = b0
// a1 = 2(ohm²−1)/c，a2 = (1−√2·ohm+ohm²)/c

export function LowPass2pBodePlot() {
  const [fc, setFc] = useState(50);
  const [fs, setFs] = useState(1000);

  const freqs = useMemo(() => logFreqs(fs), [fs]);
  const labels = useMemo(() => freqs.map((f) => f.toFixed(4)), [freqs]);

  const { mags, phases, fc_c } = useMemo(() => {
    const fc_c = Math.min(fc, fs * 0.4);
    const ohm = Math.tan((Math.PI * fc_c) / fs);
    const c = 1 + Math.SQRT2 * ohm + ohm * ohm;
    const b0 = (ohm * ohm) / c;
    const b1 = 2 * b0;
    const b2 = b0;
    const a1 = (2 * (ohm * ohm - 1)) / c;
    const a2 = (1 - Math.SQRT2 * ohm + ohm * ohm) / c;

    const resp = biquadResp(freqs, fs, b0, b1, b2, a1, a2);
    return {
      mags: resp.map((r) => r.magDb),
      phases: resp.map((r) => r.phase),
      fc_c,
    };
  }, [fc, fs, freqs]);

  return (
    <div style={CARD}>
      <div style={ROW}>
        <FreqSlider
          label="截止频率 fc"
          value={fc}
          min={0.1}
          max={Math.floor(fs * 0.4 - 1)}
          desc="上限钳位到 0.4·fs（Butterworth 设计约束）"
          onChange={setFc}
        />
        <NumInput
          label={<>采样率 f<sub>s</sub> (Hz)</>}
          value={fs}
          min={10}
          max={100000}
          step={100}
          onChange={setFs}
        />
        <Stat>
          实际 fc（已钳位）{" "}
          <strong style={{ color: "rgb(99,102,241)" }}>{fmtHz(fc_c)}</strong>
          <br />
          Butterworth: −3 dB @ fc
          <br />
          滚降 −40 dB/decade
        </Stat>
      </div>
      <BodePlot
        labels={labels}
        magDatasets={[magMainDS(mags), neg3dBLine(N)]}
        phaseDatasets={[phaseMainDS(phases)]}
        phaseMin={-200}
        phaseMax={5}
      />
    </div>
  );
}

// ─── 3. 陷波滤波器（NotchFilter） ────────────────────────────────────────────
// A = 10^(−att/40)，Q 由带宽推导
// 系数见 NotchFilter.cpp init_with_A_and_Q()

function computeNotchCoeffs(
  fc: number,
  bw: number,
  att: number,
  fs: number,
): { b0: number; b1: number; b2: number; a1: number; a2: number; valid: boolean } {
  if (fc <= bw / 2 || fc >= fs / 2)
    return { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0, valid: false };

  const A = Math.pow(10, -att / 40);
  const octaves = Math.log2(fc / (fc - bw / 2)) * 2;
  const pow2 = Math.pow(2, octaves);
  const Q = Math.sqrt(pow2) / (pow2 - 1);

  const omega = (2 * Math.PI * fc) / fs;
  const alpha = Math.sin(omega) / (2 * Q);
  const a0_inv = 1 / (1 + alpha);

  return {
    b0: (1 + alpha * A * A) * a0_inv,
    b1: (-2 * Math.cos(omega)) * a0_inv,
    b2: (1 - alpha * A * A) * a0_inv,
    a1: (-2 * Math.cos(omega)) * a0_inv,
    a2: (1 - alpha) * a0_inv,
    valid: true,
  };
}

export function NotchFilterBodePlot() {
  const [fc, setFc] = useState(50);
  const [bw, setBw] = useState(10);
  const [att, setAtt] = useState(40);
  const [fs, setFs] = useState(1000);

  const freqs = useMemo(() => logFreqs(fs), [fs]);
  const labels = useMemo(() => freqs.map((f) => f.toFixed(4)), [freqs]);

  const { mags, phases, notchDb, valid } = useMemo(() => {
    const { b0, b1, b2, a1, a2, valid } = computeNotchCoeffs(fc, bw, att, fs);
    if (!valid)
      return { mags: new Array(N).fill(0), phases: new Array(N).fill(0), notchDb: 0, valid: false };

    const resp = biquadResp(freqs, fs, b0, b1, b2, a1, a2);
    // 找最小幅度（陷波最深处）
    const allMags = resp.map((r) => r.magDb);
    const notchDb = Math.min(...allMags);
    return { mags: allMags, phases: resp.map((r) => r.phase), notchDb, valid: true };
  }, [fc, bw, att, fs, freqs]);

  const bwMax = Math.floor(fc * 1.9);

  return (
    <div style={CARD}>
      <div style={ROW}>
        <FreqSlider
          label="中心频率 fc"
          value={fc}
          min={0.1}
          max={Math.floor(fs / 2 - 1)}
          onChange={(v) => { setFc(v); if (bw >= v * 1.9) setBw(Math.max(0.1, v * 1.8)); }}
        />
        <FreqSlider
          label="带宽 bw"
          value={bw}
          min={0.1}
          max={bwMax > 0 ? bwMax : 0.1}
          desc="陷波的 −3 dB 带宽"
          onChange={setBw}
        />
        <Slider
          label="衰减"
          value={att}
          min={5}
          max={80}
          step={1}
          unit=" dB"
          onChange={setAtt}
        />
        <NumInput
          label={<>采样率 f<sub>s</sub> (Hz)</>}
          value={fs}
          min={10}
          max={100000}
          step={100}
          onChange={setFs}
        />
        <Stat>
          {valid ? (
            <>
              实际衰减（最深处）
              <br />
              <strong style={{ color: "rgb(99,102,241)" }}>{notchDb.toFixed(1)} dB</strong>
            </>
          ) : (
            <span style={{ color: "rgb(239,68,68)" }}>⚠ 参数无效：fc 须 &gt; bw/2</span>
          )}
        </Stat>
      </div>
      <BodePlot
        labels={labels}
        magDatasets={[magMainDS(mags), neg3dBLine(N)]}
        phaseDatasets={[phaseMainDS(phases)]}
        phaseMin={-200}
        phaseMax={200}
      />
    </div>
  );
}

// ─── 4. 谐波陷波滤波器（HarmonicNotchFilter） ────────────────────────────────
// 多个 NotchFilter 串联，中心频率为基频的整数倍
// 总响应 = 各陷波响应之积（dB 上为各 dB 之和）

export function HarmonicNotchBodePlot() {
  const [fc, setFc] = useState(50);
  const [bw, setBw] = useState(10);
  const [att, setAtt] = useState(40);
  const [fs, setFs] = useState(1000);
  const [h1, setH1] = useState(true);
  const [h2, setH2] = useState(true);
  const [h3, setH3] = useState(false);
  const [h4, setH4] = useState(false);

  const freqs = useMemo(() => logFreqs(fs), [fs]);
  const labels = useMemo(() => freqs.map((f) => f.toFixed(4)), [freqs]);

  const { mags, phases } = useMemo(() => {
    const harmonics = [
      { k: 1, on: h1 },
      { k: 2, on: h2 },
      { k: 3, on: h3 },
      { k: 4, on: h4 },
    ].filter((h) => h.on);

    const mags = new Array(N).fill(0);
    const phases = new Array(N).fill(0);

    for (const { k } of harmonics) {
      const center = fc * k;
      const { b0, b1, b2, a1, a2, valid } = computeNotchCoeffs(center, bw * k, att, fs);
      if (!valid) continue;
      const resp = biquadResp(freqs, fs, b0, b1, b2, a1, a2);
      for (let i = 0; i < N; i++) {
        mags[i] += resp[i].magDb;
        phases[i] += resp[i].phase;
      }
    }

    return { mags, phases };
  }, [fc, bw, att, fs, h1, h2, h3, h4, freqs]);

  const bwMax = Math.floor(fc * 1.9);
  const CheckBox = ({
    label,
    checked,
    onChange,
  }: {
    label: string;
    checked: boolean;
    onChange: (v: boolean) => void;
  }) => (
    <label
      style={{
        display: "flex",
        alignItems: "center",
        gap: 6,
        cursor: "pointer",
        fontWeight: 500,
        fontSize: "0.9rem",
      }}
    >
      <input
        type="checkbox"
        checked={checked}
        onChange={(e) => onChange(e.target.checked)}
        style={{ width: 16, height: 16 }}
      />
      {label}
    </label>
  );

  return (
    <div style={CARD}>
      <div style={ROW}>
        <FreqSlider
          label="基频 fc"
          value={fc}
          min={0.1}
          max={Math.floor(fs / 8)}
          desc="通常对应电机转速基频"
          onChange={(v) => { setFc(v); if (bw >= v * 1.9) setBw(Math.max(0.1, v * 1.8)); }}
        />
        <FreqSlider
          label="带宽（基频处）"
          value={bw}
          min={0.1}
          max={bwMax > 0 ? bwMax : 0.1}
          desc="各次谐波的带宽按比例缩放"
          onChange={setBw}
        />
        <Slider
          label="衰减"
          value={att}
          min={5}
          max={80}
          step={1}
          unit=" dB"
          onChange={setAtt}
        />
        <NumInput
          label={<>采样率 f<sub>s</sub> (Hz)</>}
          value={fs}
          min={10}
          max={100000}
          step={100}
          onChange={setFs}
        />
      </div>
      <div style={{ display: "flex", gap: "1.5rem", flexWrap: "wrap", marginBottom: "1rem" }}>
        <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>抑制谐波：</span>
        <CheckBox label={`1× (${fmtHz(fc)})`} checked={h1} onChange={setH1} />
        <CheckBox label={`2× (${fmtHz(fc * 2)})`} checked={h2} onChange={setH2} />
        <CheckBox label={`3× (${fmtHz(fc * 3)})`} checked={h3} onChange={setH3} />
        <CheckBox label={`4× (${fmtHz(fc * 4)})`} checked={h4} onChange={setH4} />
      </div>
      <BodePlot
        labels={labels}
        magDatasets={[magMainDS(mags), neg3dBLine(N)]}
        phaseDatasets={[phaseMainDS(phases)]}
        phaseMin={-400}
        phaseMax={200}
      />
    </div>
  );
}

// ─── 5. 滑动平均滤波器（AverageFilter） ──────────────────────────────────────
// H(e^{jθ}) = (1/N)·sin(N·θ/2)/sin(θ/2)·e^{−j(N−1)θ/2}
// |H(f)|    = |sin(N·π·f/fs)| / (N·|sin(π·f/fs)|)
// ∠H(f)    = −(N−1)·π·f/fs

export function AverageFilterBodePlot() {
  const [navg, setNavg] = useState(4);
  const [fs, setFs] = useState(1000);

  const freqs = useMemo(() => logFreqs(fs), [fs]);
  const labels = useMemo(() => freqs.map((f) => f.toFixed(4)), [freqs]);

  const { mags, phases, nullFreq } = useMemo(() => {
    const mags: number[] = [];
    const phases: number[] = [];
    for (const f of freqs) {
      const θ = (Math.PI * f) / fs;
      const num = Math.abs(Math.sin(navg * θ));
      const den = navg * Math.abs(Math.sin(θ));
      const mag = den < 1e-12 ? 1 : num / den;
      mags.push(20 * Math.log10(Math.max(mag, 1e-12)));
      phases.push((-(navg - 1) * θ * 180) / Math.PI);
    }
    return { mags, phases, nullFreq: fs / navg };
  }, [navg, fs, freqs]);

  return (
    <div style={CARD}>
      <div style={ROW}>
        <label style={{ display: "flex", flexDirection: "column", gap: 4, flex: "0 0 auto" }}>
          <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>
            滤波器大小 N ={" "}
            <span style={{ color: "rgb(99,102,241)" }}>{navg}</span>
          </span>
          <select
            value={navg}
            onChange={(e) => setNavg(parseInt(e.target.value, 10))}
            style={{ ...INPUT_STYLE, width: "auto" }}
          >
            {[2, 3, 4, 5].map((v) => (
              <option key={v} value={v}>
                {v} 点平均
              </option>
            ))}
          </select>
        </label>
        <NumInput
          label={<>采样率 f<sub>s</sub> (Hz)</>}
          value={fs}
          min={10}
          max={100000}
          step={100}
          onChange={setFs}
        />
        <Stat>
          第一零点（nulling）
          <br />
          <strong style={{ color: "rgb(99,102,241)" }}>{fmtHz(nullFreq)}</strong>
          <br />
          群延迟 ≈ {((navg - 1) / 2 / fs * 1000).toFixed(2)} ms
        </Stat>
      </div>
      <BodePlot
        labels={labels}
        magDatasets={[magMainDS(mags), neg3dBLine(N)]}
        phaseDatasets={[phaseMainDS(phases)]}
        phaseMin={-200}
        phaseMax={5}
      />
    </div>
  );
}

// ─── 6. 平滑微分滤波器（DerivativeFilter，Holoborodko） ──────────────────────
// 纯虚数传递函数（相位恒为 +90°）
// SIZE 5:  H = j·fs·(2·sin ω + sin 2ω)/4
// SIZE 7:  H = j·fs·(5·sin ω + 4·sin 2ω + sin 3ω)/16
// SIZE 9:  H = j·fs·(14·sin ω + 14·sin 2ω + 6·sin 3ω + sin 4ω)/64
// SIZE 11: H = j·fs·(42·sin ω + 48·sin 2ω + 27·sin 3ω + 8·sin 4ω + sin 5ω)/256
//
// 参考线：理想微分器增益 2π·f（以 dB 显示）

const DERIV_CONFIGS: Record<
  number,
  { weights: number[]; scale: number }
> = {
  5: { weights: [2, 1], scale: 4 },
  7: { weights: [5, 4, 1], scale: 16 },
  9: { weights: [14, 14, 6, 1], scale: 64 },
  11: { weights: [42, 48, 27, 8, 1], scale: 256 },
};

export function DerivativeFilterBodePlot() {
  const [size, setSize] = useState(7);
  const [fs, setFs] = useState(1000);

  const freqs = useMemo(() => logFreqs(fs), [fs]);
  const labels = useMemo(() => freqs.map((f) => f.toFixed(4)), [freqs]);

  const { mags, idealMags } = useMemo(() => {
    const cfg = DERIV_CONFIGS[size];
    const mags: number[] = [];
    const idealMags: number[] = [];

    for (const f of freqs) {
      const ω = (2 * Math.PI * f) / fs;
      // |H| = fs * Σ(w_k · sin(k·ω)) / scale
      let sum = 0;
      for (let i = 0; i < cfg.weights.length; i++) {
        sum += cfg.weights[i] * Math.sin((i + 1) * ω);
      }
      const gainLinear = (fs * sum) / cfg.scale;
      mags.push(20 * Math.log10(Math.max(gainLinear, 1e-12)));

      // 理想微分器增益 = 2π·f
      const idealGain = 2 * Math.PI * f;
      idealMags.push(20 * Math.log10(Math.max(idealGain, 1e-12)));
    }

    return { mags, idealMags };
  }, [size, fs, freqs]);

  const magMax = useMemo(() => {
    const allFinite = [...mags, ...idealMags].filter(isFinite);
    return allFinite.length > 0
      ? Math.ceil(Math.max(...allFinite) / 10) * 10 + 5
      : 60;
  }, [mags, idealMags]);

  // 计算有效微分带宽：实际增益与理想增益相差不超过 3 dB 的最高频率
  const effectiveBw = useMemo(() => {
    let last = 0;
    for (let i = 0; i < N; i++) {
      const diff = Math.abs(mags[i] - idealMags[i]);
      if (diff <= 3) last = freqs[i];
      else if (freqs[i] > fs / 4) break; // 高频区不再计入
    }
    return last;
  }, [mags, idealMags, freqs, fs]);

  const phaseData = new Array(N).fill(90); // 纯虚数 → 相位恒为 +90°

  const idealDS: DS = {
    label: "理想微分器",
    data: idealMags,
    borderColor: "rgba(234,88,12,0.7)",
    borderWidth: 1.5,
    borderDash: [6, 4],
    pointRadius: 0,
    fill: false,
    tension: 0,
  };

  const ref90DS: DS = {
    label: "+90°",
    data: new Array(N).fill(90),
    borderColor: "rgba(239,68,68,0.6)",
    borderWidth: 1.5,
    borderDash: [6, 4],
    pointRadius: 0,
    fill: false,
    tension: 0,
  };

  return (
    <div style={CARD}>
      <div style={ROW}>
        <label style={{ display: "flex", flexDirection: "column", gap: 4, flex: "0 0 auto" }}>
          <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>
            缓冲区大小 SIZE ={" "}
            <span style={{ color: "rgb(99,102,241)" }}>{size}</span>
          </span>
          <select
            value={size}
            onChange={(e) => setSize(parseInt(e.target.value, 10))}
            style={{ ...INPUT_STYLE, width: "auto" }}
          >
            {[5, 7, 9, 11].map((v) => (
              <option key={v} value={v}>
                SIZE = {v}
              </option>
            ))}
          </select>
          <span style={{ fontSize: "0.75rem", color: "var(--ifm-color-emphasis-600)" }}>
            越大 → 噪声抑制越强，高频准确性越好
          </span>
        </label>
        <NumInput
          label={<>采样率 f<sub>s</sub> (Hz)</>}
          value={fs}
          min={10}
          max={100000}
          step={100}
          onChange={setFs}
        />
        <Stat>
          有效微分带宽（≤ 3 dB 误差）
          <br />
          <strong style={{ color: "rgb(99,102,241)" }}>
            {effectiveBw > 0 ? fmtHz(effectiveBw) : "—"}
          </strong>
          <br />
          <span style={{ fontSize: "0.8rem" }}>
            曲线偏离橙色参考线处即为滤波器引入的误差
          </span>
        </Stat>
      </div>
      <BodePlot
        labels={labels}
        magDatasets={[magMainDS(mags), idealDS]}
        phaseDatasets={[phaseMainDS(phaseData), ref90DS]}
        magMax={magMax}
        phaseMin={-10}
        phaseMax={100}
      />
    </div>
  );
}
