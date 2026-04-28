import React, { useState, useMemo } from "react";
import {
  Chart as ChartJS,
  LogarithmicScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from "chart.js";
import { Line } from "react-chartjs-2";

ChartJS.register(
  LogarithmicScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
);

const N_POINTS = 200;

function computeBode(alpha: number, fs: number) {
  const beta = 1 - alpha;
  const fMax = fs / 2;
  const fMin = Math.max(0.1, fMax / 1e4);

  const freqs: number[] = [];
  const magnitudes: number[] = [];
  const phases: number[] = [];

  for (let i = 0; i < N_POINTS; i++) {
    const f = fMin * Math.pow(fMax / fMin, i / (N_POINTS - 1));
    const theta = (2 * Math.PI * f) / fs;
    const re = 1 - beta * Math.cos(theta);
    const im = beta * Math.sin(theta);
    const mag = alpha / Math.sqrt(re * re + im * im);
    const magDb = 20 * Math.log10(mag);
    const phaseRad = -Math.atan2(im, re);
    const phaseDeg = (phaseRad * 180) / Math.PI;

    freqs.push(f);
    magnitudes.push(magDb);
    phases.push(phaseDeg);
  }

  return { freqs, magnitudes, phases };
}

/**
 * 精确解析 -3dB 带宽
 * 令 |H(f)|² = 0.5，由 H(z) 展开得：
 *   cos θ_c = (1 + β² - 2α²) / (2β)，f_bw = θ_c·fs/(2π)
 */
function computeBandwidth(alpha: number, fs: number): number {
  if (alpha >= 1.0) return fs / 2;
  const beta = 1 - alpha;
  const cosTheta = (1 + beta * beta - 2 * alpha * alpha) / (2 * beta);
  if (cosTheta <= -1) return fs / 2;
  if (cosTheta >= 1) return 0;
  return (Math.acos(cosTheta) * fs) / (2 * Math.PI);
}

const CHART_HEIGHT = 280;

const commonOptions = (yLabel: string, yMin: number, yMax: number) => ({
  responsive: true,
  animation: { duration: 0 } as const,
  interaction: { mode: "index" as const, intersect: false },
  plugins: {
    legend: { display: false },
    tooltip: {
      callbacks: {
        title: (items: { label: string }[]) => {
          const f = parseFloat(items[0].label);
          return f >= 1000
            ? `${(f / 1000).toFixed(2)} kHz`
            : `${f.toFixed(2)} Hz`;
        },
        label: (item: { formattedValue: string }) =>
          `${item.formattedValue} ${yLabel.split(" ")[1] ?? ""}`,
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
          if ([0.1, 1, 10, 100, 1000, 10000].includes(v))
            return v >= 1000 ? `${v / 1000}k` : String(v);
          return null;
        },
      },
    },
    y: {
      title: { display: true, text: yLabel },
      min: yMin,
      max: yMax,
    },
  },
});

export default function DiffFilterBodePlot() {
  const [alpha, setAlpha] = useState(0.5);
  const [fs, setFs] = useState(1000);

  const { freqs, magnitudes, phases } = useMemo(
    () => computeBode(alpha, fs),
    [alpha, fs],
  );

  const bandwidth = useMemo(() => computeBandwidth(alpha, fs), [alpha, fs]);

  const labels = freqs.map((f) => f.toFixed(4));

  const magData = {
    labels,
    datasets: [
      {
        label: "幅度",
        data: magnitudes,
        borderColor: "rgb(99, 102, 241)",
        backgroundColor: "rgba(99, 102, 241, 0.1)",
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.1,
        fill: true,
      },
      {
        label: "-3 dB",
        data: new Array(N_POINTS).fill(-3),
        borderColor: "rgba(239, 68, 68, 0.85)",
        backgroundColor: "transparent",
        borderWidth: 1.5,
        borderDash: [6, 4],
        pointRadius: 0,
        fill: false,
        tension: 0,
      },
    ],
  };

  const phaseData = {
    labels,
    datasets: [
      {
        data: phases,
        borderColor: "rgb(234, 88, 12)",
        backgroundColor: "rgba(234, 88, 12, 0.1)",
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.1,
        fill: true,
      },
    ],
  };

  const magMin = Math.floor(Math.min(...magnitudes) / 10) * 10 - 10;

  return (
    <div
      style={{
        border: "1px solid var(--ifm-color-emphasis-300)",
        borderRadius: 8,
        padding: "1.25rem",
        marginBottom: "1.5rem",
      }}
    >
      {/* Controls */}
      <div
        style={{
          display: "flex",
          flexWrap: "wrap",
          gap: "1.5rem",
          marginBottom: "1.25rem",
          alignItems: "center",
        }}
      >
        <label style={{ display: "flex", flexDirection: "column", gap: 4, flex: "1 1 200px" }}>
          <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>
            α (diff_lpf_alpha) = <span style={{ color: "rgb(99,102,241)" }}>{alpha.toFixed(2)}</span>
          </span>
          <input
            type="range"
            min={0.01}
            max={1.0}
            step={0.01}
            value={alpha}
            onChange={(e) => setAlpha(parseFloat(e.target.value))}
            style={{ width: "100%" }}
          />
          <span style={{ fontSize: "0.75rem", color: "var(--ifm-color-emphasis-600)" }}>
            0.01（滤波最强）→ 1.00（不滤波）
          </span>
        </label>

        <label style={{ display: "flex", flexDirection: "column", gap: 4, flex: "0 0 auto" }}>
          <span style={{ fontWeight: 600, fontSize: "0.9rem" }}>
            采样率 f<sub>s</sub> (Hz)
          </span>
          <input
            type="number"
            min={10}
            max={100000}
            step={100}
            value={fs}
            onChange={(e) => {
              const v = parseInt(e.target.value, 10);
              if (v >= 10) setFs(v);
            }}
            style={{
              width: 120,
              padding: "0.3rem 0.5rem",
              border: "1px solid var(--ifm-color-emphasis-300)",
              borderRadius: 4,
              background: "var(--ifm-background-color)",
              color: "var(--ifm-font-color-base)",
            }}
          />
        </label>

        <div style={{ fontSize: "0.85rem", color: "var(--ifm-color-emphasis-700)", flex: "1 1 160px" }}>
          系统带宽（-3 dB）{" "}
          <strong style={{ color: "rgb(99,102,241)" }}>
            {bandwidth >= 1000
              ? `${(bandwidth / 1000).toFixed(2)} kHz`
              : `${bandwidth.toFixed(1)} Hz`}
          </strong>
          <br />
          奈奎斯特频率 {(fs / 2).toFixed(0)} Hz
        </div>
      </div>

      {/* Magnitude plot */}
      <div style={{ marginBottom: "0.5rem" }}>
        <div style={{ fontSize: "0.8rem", fontWeight: 600, marginBottom: 4, color: "var(--ifm-color-emphasis-700)" }}>
          幅频响应
        </div>
        <div style={{ height: CHART_HEIGHT }}>
          {(() => {
            const base = commonOptions("幅度 (dB)", magMin, 3);
            return (
              <Line
                data={magData}
                options={{
                  ...base,
                  plugins: {
                    ...base.plugins,
                    legend: {
                      display: true,
                      position: "top" as const,
                      labels: { boxWidth: 20, font: { size: 12 } },
                    },
                  },
                  maintainAspectRatio: false,
                }}
              />
            );
          })()}
        </div>
      </div>

      {/* Phase plot */}
      <div>
        <div style={{ fontSize: "0.8rem", fontWeight: 600, marginBottom: 4, color: "var(--ifm-color-emphasis-700)" }}>
          相频响应
        </div>
        <div style={{ height: CHART_HEIGHT }}>
          <Line
            data={phaseData}
            options={{
              ...commonOptions("相位 (°)", -90, 0),
              maintainAspectRatio: false,
            }}
          />
        </div>
      </div>
    </div>
  );
}
