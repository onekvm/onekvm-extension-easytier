import type { Translator } from "./i18n";
import type { Status } from "./types";

function text(value: unknown, fallback: string): string {
  return typeof value === "string" && value ? value : fallback;
}

export function statusLabel(state: Status, t: Translator): string {
  return t(state.ready === true ? "running" : "notRunning");
}

export function summary(state: Status, t: Translator): string {
  return state.ready === true ? text(state.ipv4, t("ready")) : t("notReady");
}
