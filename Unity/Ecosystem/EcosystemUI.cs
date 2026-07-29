using UnityEngine;
using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Enhanced UI displaying ecosystem population stats and evolution progress.
    /// Shows real-time graphs and learning indicators.
    /// </summary>
    public class EcosystemUI : MonoBehaviour
    {
        [SerializeField] private bool _showUI = true;
        [SerializeField] private Vector2 _position = new Vector2(10, 10);
        
        private GUIStyle _boxStyle;
        private GUIStyle _labelStyle;
        private GUIStyle _headerStyle;
        private GUIStyle _smallStyle;
        private Texture2D _graphBg;
        private Texture2D _graphLine;

        private void OnGUI()
        {
            if (!_showUI || EcosystemManager.Instance == null) return;

            InitStyles();

            var manager = EcosystemManager.Instance;
            var evolution = EvolutionManager.Instance;
            
            float width = 280;
            float height = evolution != null ? 320 : 120;
            Rect boxRect = new Rect(_position.x, _position.y, width, height);
            
            GUI.Box(boxRect, "", _boxStyle);
            
            GUILayout.BeginArea(new Rect(boxRect.x + 10, boxRect.y + 10, width - 20, height - 20));
            
            // Header with elapsed time
            if (evolution != null)
            {
                int minutes = (int)(evolution.ElapsedTime / 60);
                int seconds = (int)(evolution.ElapsedTime % 60);
                GUILayout.Label($"🌿 Ecosystem [{minutes:00}:{seconds:00}]", _headerStyle);
            }
            else
            {
                GUILayout.Label("🌿 Ecosystem", _headerStyle);
            }
            GUILayout.Space(3);
            
            // Prey bar
            DrawPopulationBar("🐟 Prey", manager.PreyCount, manager.MaxPrey, new Color(0.2f, 0.6f, 1f));
            
            // Predator bar
            DrawPopulationBar("🦈 Predators", manager.PredatorCount, manager.MaxPredators, new Color(1f, 0.3f, 0.3f));
            
            // Food count
            GUILayout.Label($"🍎 Food: {manager.FoodCount}", _labelStyle);
            
            // Evolution stats
            if (evolution != null)
            {
                GUILayout.Space(8);
                GUILayout.Label("🧬 EVOLUTION LEARNING", _headerStyle);
                
                // Generation counters with prominent display
                GUILayout.BeginHorizontal();
                GUI.color = new Color(0.3f, 0.7f, 1f);
                GUILayout.Label($"Prey Gen: {evolution.PreyGeneration}", _labelStyle, GUILayout.Width(100));
                GUI.color = new Color(1f, 0.4f, 0.3f);
                GUILayout.Label($"Hunter Gen: {evolution.PredatorGeneration}", _labelStyle);
                GUI.color = Color.white;
                GUILayout.EndHorizontal();
                
                // Current vs Best comparison
                GUILayout.Space(5);
                DrawStatComparison("Prey Survival", evolution.AvgPreySurvival, evolution.BestPreySurvival, "s", new Color(0.3f, 0.7f, 1f));
                DrawStatComparison("Hunter Kills", evolution.AvgPredatorKills, evolution.BestPredatorKills, "", new Color(1f, 0.4f, 0.3f));
                
                // Progress graph
                GUILayout.Space(8);
                GUILayout.Label("📈 Survival Progress", _smallStyle);
                DrawProgressGraph(evolution.PreySurvivalHistory, new Color(0.3f, 0.7f, 1f), 60);
                
                // Learning indicator
                GUILayout.Space(5);
                DrawLearningIndicator(evolution);
            }
            
            GUILayout.EndArea();
        }

        private void DrawStatComparison(string label, float current, float best, string unit, Color color)
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label($"{label}:", _smallStyle, GUILayout.Width(90));
            GUI.color = color;
            GUILayout.Label($"Avg: {current:F1}{unit}", _smallStyle, GUILayout.Width(70));
            GUI.color = new Color(1f, 0.9f, 0.3f); // Gold for best
            GUILayout.Label($"Best: {best:F1}{unit}", _smallStyle);
            GUI.color = Color.white;
            GUILayout.EndHorizontal();
        }

        private void DrawProgressGraph(List<float> history, Color color, float height)
        {
            Rect graphRect = GUILayoutUtility.GetRect(250, height);
            
            // Background
            GUI.color = new Color(0.05f, 0.05f, 0.1f, 0.8f);
            GUI.DrawTexture(graphRect, Texture2D.whiteTexture);
            
            if (history == null || history.Count < 2)
            {
                GUI.color = new Color(0.5f, 0.5f, 0.5f);
                GUI.Label(graphRect, "Waiting for data...", _smallStyle);
                GUI.color = Color.white;
                return;
            }
            
            // Find max for scaling
            float maxVal = 1f;
            foreach (float v in history) if (v > maxVal) maxVal = v;
            
            // Draw grid lines
            GUI.color = new Color(0.2f, 0.2f, 0.3f);
            for (int i = 1; i < 4; i++)
            {
                float y = graphRect.y + (graphRect.height * i / 4);
                GUI.DrawTexture(new Rect(graphRect.x, y, graphRect.width, 1), Texture2D.whiteTexture);
            }
            
            // Draw line graph
            GUI.color = color;
            float pointWidth = graphRect.width / (history.Count - 1);
            
            for (int i = 0; i < history.Count - 1; i++)
            {
                float x1 = graphRect.x + i * pointWidth;
                float x2 = graphRect.x + (i + 1) * pointWidth;
                float y1 = graphRect.y + graphRect.height - (history[i] / maxVal * graphRect.height * 0.9f);
                float y2 = graphRect.y + graphRect.height - (history[i + 1] / maxVal * graphRect.height * 0.9f);
                
                // Draw thick line using boxes
                DrawLine(x1, y1, x2, y2, 2f);
            }
            
            // Draw current value
            if (history.Count > 0)
            {
                float lastVal = history[history.Count - 1];
                GUI.color = Color.white;
                float textY = graphRect.y + graphRect.height - (lastVal / maxVal * graphRect.height * 0.9f) - 10;
                GUI.Label(new Rect(graphRect.x + graphRect.width - 40, textY, 40, 20), $"{lastVal:F1}s", _smallStyle);
            }
            
            GUI.color = Color.white;
        }

        private void DrawLine(float x1, float y1, float x2, float y2, float thickness)
        {
            float angle = Mathf.Atan2(y2 - y1, x2 - x1) * Mathf.Rad2Deg;
            float length = Vector2.Distance(new Vector2(x1, y1), new Vector2(x2, y2));
            
            GUIUtility.RotateAroundPivot(angle, new Vector2(x1, y1));
            GUI.DrawTexture(new Rect(x1, y1 - thickness/2, length, thickness), Texture2D.whiteTexture);
            GUIUtility.RotateAroundPivot(-angle, new Vector2(x1, y1));
        }

        private void DrawLearningIndicator(EvolutionManager evolution)
        {
            string status;
            Color statusColor;
            
            if (evolution.PreyGeneration < 3)
            {
                status = "🔄 INITIALIZING - Random behaviors";
                statusColor = new Color(0.7f, 0.7f, 0.7f);
            }
            else if (evolution.PreyGeneration < 10)
            {
                status = "📊 LEARNING - Selecting survivors";
                statusColor = new Color(1f, 0.8f, 0.3f);
            }
            else if (evolution.PreyGeneration < 20)
            {
                status = "🧠 EVOLVING - Strategies emerging";
                statusColor = new Color(0.3f, 0.9f, 0.5f);
            }
            else
            {
                status = "⚡ OPTIMIZED - Arms race active";
                statusColor = new Color(0.5f, 0.8f, 1f);
            }
            
            GUI.color = statusColor;
            GUILayout.Label(status, _smallStyle);
            GUI.color = Color.white;
        }

        private void DrawPopulationBar(string label, int current, int max, Color color)
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label($"{label}: {current}/{max}", _labelStyle, GUILayout.Width(140));
            
            // Progress bar
            Rect barRect = GUILayoutUtility.GetRect(100, 14);
            float percent = (float)current / max;
            
            GUI.color = new Color(0.15f, 0.15f, 0.2f, 0.8f);
            GUI.DrawTexture(barRect, Texture2D.whiteTexture);
            
            GUI.color = color;
            barRect.width *= percent;
            GUI.DrawTexture(barRect, Texture2D.whiteTexture);
            
            GUI.color = Color.white;
            GUILayout.EndHorizontal();
        }

        private void InitStyles()
        {
            if (_boxStyle != null) return;

            _boxStyle = new GUIStyle(GUI.skin.box);
            _boxStyle.normal.background = MakeTexture(2, 2, new Color(0.08f, 0.08f, 0.12f, 0.95f));

            _labelStyle = new GUIStyle(GUI.skin.label);
            _labelStyle.normal.textColor = Color.white;
            _labelStyle.fontSize = 12;

            _headerStyle = new GUIStyle(_labelStyle);
            _headerStyle.fontSize = 13;
            _headerStyle.fontStyle = FontStyle.Bold;
            
            _smallStyle = new GUIStyle(_labelStyle);
            _smallStyle.fontSize = 10;
        }

        private Texture2D MakeTexture(int width, int height, Color color)
        {
            Color[] pixels = new Color[width * height];
            for (int i = 0; i < pixels.Length; i++)
            {
                pixels[i] = color;
            }
            Texture2D tex = new Texture2D(width, height);
            tex.SetPixels(pixels);
            tex.Apply();
            return tex;
        }
    }
}
