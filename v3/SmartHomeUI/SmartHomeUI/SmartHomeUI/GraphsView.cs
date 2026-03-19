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
    private readonly Label _locationLabel;
    private readonly SmartHomeService _service;

    private int _currentGraphIndex;
    private List<SensorData>? _data;

    internal Graph(SmartHomeService service, string title)
    {
        _service = service;
        Orientation = Orientation.Vertical;
        _plot = new EtoPlot();
        _plot.Plot.Title(title);
        _nextButton = new Button { Text = ">" };
        _nextButton.Click += (_, _) => NextGraph();
        _locationLabel = new Label();
        Items.Add(new StackLayout { Orientation = Orientation.Horizontal, Items = { _plot, _nextButton } });
        Items.Add(_locationLabel);
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
        if (_data == null || _data.Count == 0)
        {
            _locationLabel.Text = "";
            return;
        }
        var currentData = _data[_currentGraphIndex];
        _locationLabel.Text = _service.GetLocationName(currentData.SensorId);
        if (currentData.Raw != null)
            _plot.Plot.Add.Scatter(currentData.Raw.Select(item => item.DateTime).ToList(),
                currentData.Raw.Select(item => item.Value).ToList());
        else
        {
            _plot.Plot.Add.Scatter(currentData.Aggregated!.Min.Select(item => item.DateTime).ToList(),
                currentData.Aggregated.Min.Select(item => item.Value).ToList());
            _plot.Plot.Add.Scatter(currentData.Aggregated!.Avg.Select(item => item.DateTime).ToList(),
                currentData.Aggregated.Avg.Select(item => item.Value).ToList());
            _plot.Plot.Add.Scatter(currentData.Aggregated!.Max.Select(item => item.DateTime).ToList(),
                currentData.Aggregated.Max.Select(item => item.Value).ToList());
        }
        _plot.Plot.Axes.DateTimeTicksBottom();
    }

    internal void SetData(List<SensorData>? data)
    {
        _currentGraphIndex = 0;
        _data = data;
        BuildGraph();
    }
}

internal abstract class GraphsView : StackLayout
{
    protected readonly Graph[] Graphs;
    protected readonly SmartHomeService Service;

    protected GraphsView(SmartHomeService service, string[] titles)
    {
        Service = service;
        Orientation = Orientation.Vertical;
        Graphs = new Graph[titles.Length];
        for (var i = 0; i < titles.Length; i++)
        {
            Graphs[i] = new Graph(service, titles[i]);
            Items.Add(Graphs[i]);
        }
    }

    internal abstract void Refresh(Dictionary<string, List<SensorData>> data);
}

internal sealed class EnvGraphsView : GraphsView
{
    private readonly Graph _intTempGraph;
    private readonly Graph _extTempGraph;
    private readonly Graph _humiGraph;
    private readonly Graph _presGraph;
    private readonly Graph _co2Graph;
    private readonly Graph _luxGraph;

    internal EnvGraphsView(SmartHomeService service) : base(service, ["Temperatute", "Temperature", "Humidity", "Pressure", "CO2", "Luminosity"])
    {
        _intTempGraph = Graphs[0];
        _extTempGraph = Graphs[1];
        _humiGraph = Graphs[2];
        _presGraph = Graphs[3];
        _co2Graph = Graphs[4];
        _luxGraph = Graphs[5];
    }

    internal override void Refresh(Dictionary<string, List<SensorData>> data)
    {
        var tempData = data.GetValueOrDefault("temp");
        _intTempGraph.SetData(tempData?.Where(sd => !Service.IsExtSensor(sd.SensorId)).ToList());
        _extTempGraph.SetData(tempData?.Where(sd => Service.IsExtSensor(sd.SensorId)).ToList());
        _humiGraph.SetData(data.GetValueOrDefault("humi"));
        _presGraph.SetData(data.GetValueOrDefault("pres"));
        _co2Graph.SetData(data.GetValueOrDefault("co2 "));
        _luxGraph.SetData(data.GetValueOrDefault("lux "));
    }
}

internal sealed class WatGraphsView : GraphsView
{
    private readonly Graph _pressureGraph;
    private readonly Graph _currentGraph;

    internal WatGraphsView(SmartHomeService service) : base(service, ["Pressure", "Current"])
    {
        _pressureGraph = Graphs[0];
        _currentGraph = Graphs[1];
    }

    internal override void Refresh(Dictionary<string, List<SensorData>> data)
    {
        _pressureGraph.SetData(data.GetValueOrDefault("pres"));
        _currentGraph.SetData(data.GetValueOrDefault("icc "));
    }
}

internal sealed class EleGraphsView : GraphsView
{
    private readonly Graph _powerGraph;
    private readonly Graph _voltageGraph;

    internal EleGraphsView(SmartHomeService service) : base(service, ["Power", "Voltage"])
    {
        _powerGraph = Graphs[0];
        _voltageGraph = Graphs[1];
    }

    internal override void Refresh(Dictionary<string, List<SensorData>> data)
    {
        _powerGraph.SetData(data.GetValueOrDefault("pwr "));
        _voltageGraph.SetData(data.GetValueOrDefault("vcc "));
    }
}