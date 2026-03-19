using System.Collections.Generic;
using System.Linq;
using Eto.Forms;
using ScottPlot.Eto;
using Orientation = Eto.Forms.Orientation;

namespace SmartHomeUI;

internal sealed class Graph : StackLayout
{
    private readonly EtoPlot _plot;
    private readonly Button _nextButton;
    private int _currentGraphIndex;
    private List<SensorData>? _data;

    internal Graph(string title)
    {
        Orientation = Orientation.Horizontal;
        _plot = new EtoPlot();
        _plot.Plot.Title(title);
        _nextButton = new Button { Text = ">" };
        _nextButton.Click += (_, _) => NextGraph();       
        Items.Add(_plot);
        Items.Add(_nextButton);
    }

    private void NextGraph()
    {
        if (_data == null || _data.Count < 2)
            return;
        _currentGraphIndex = _currentGraphIndex >= _data.Count ? 0 : _currentGraphIndex + 1;
        BuildGraph();
    }

    private void BuildGraph()
    {
        _plot.Plot.Clear();
    }

    internal void SetData(SmartHomeService service, List<SensorData>? data)
    {
        _currentGraphIndex = 0;
        _data = data;
        BuildGraph();
    }
}

internal abstract class GraphsView : StackLayout
{
    protected readonly Graph[] Graphs;

    protected GraphsView(string[] titles)
    {
        Orientation = Orientation.Vertical;
        Graphs = new Graph[titles.Length];
        for (var i = 0; i < titles.Length; i++)
        {
            Graphs[i] = new Graph(titles[i]);
            Items.Add(Graphs[i]);
        }
    }

    internal abstract void Refresh(SmartHomeService service, Dictionary<string, List<SensorData>> data);
}

internal sealed class EnvGraphsView : GraphsView
{
    private readonly Graph _intTempGraph;
    private readonly Graph _extTempGraph;
    private readonly Graph _humiGraph;
    private readonly Graph _presGraph;
    private readonly Graph _co2Graph;
    private readonly Graph _luxGraph;

    internal EnvGraphsView() : base(["Temperatute", "Temperature", "Humidity", "Pressure", "CO2", "Luminosity"])
    {
        _intTempGraph = Graphs[0];
        _extTempGraph = Graphs[1];
        _humiGraph = Graphs[2];
        _presGraph = Graphs[3];
        _co2Graph = Graphs[4];
        _luxGraph = Graphs[5];
    }

    internal override void Refresh(SmartHomeService service, Dictionary<string, List<SensorData>> data)
    {
        var tempData = data.GetValueOrDefault("temp");
        _intTempGraph.SetData(service, tempData?.Where(sd => !service.IsExtSensor(sd.SensorId)).ToList());
        _extTempGraph.SetData(service, tempData?.Where(sd => service.IsExtSensor(sd.SensorId)).ToList());
        _humiGraph.SetData(service, data.GetValueOrDefault("humi"));
        _presGraph.SetData(service, data.GetValueOrDefault("pres"));
        _co2Graph.SetData(service, data.GetValueOrDefault("co2 "));
        _luxGraph.SetData(service, data.GetValueOrDefault("lux "));
    }
}

internal sealed class WatGraphsView : GraphsView
{
    private readonly Graph _pressureGraph;
    private readonly Graph _currentGraph;

    internal WatGraphsView() : base(["Pressure", "Current"])
    {
        _pressureGraph = Graphs[0];
        _currentGraph = Graphs[1];
    }

    internal override void Refresh(SmartHomeService service, Dictionary<string, List<SensorData>> data)
    {
        _pressureGraph.SetData(service, data.GetValueOrDefault("pres"));
        _currentGraph.SetData(service, data.GetValueOrDefault("icc "));
    }
}

internal sealed class EleGraphsView : GraphsView
{
    private readonly Graph _powerGraph;
    private readonly Graph _voltageGraph;

    internal EleGraphsView() : base(["Power", "Voltage"])
    {
        _powerGraph = Graphs[0];
        _voltageGraph = Graphs[1];
    }

    internal override void Refresh(SmartHomeService service, Dictionary<string, List<SensorData>> data)
    {
        _powerGraph.SetData(service, data.GetValueOrDefault("pwr "));
        _voltageGraph.SetData(service, data.GetValueOrDefault("vcc "));
    }
}